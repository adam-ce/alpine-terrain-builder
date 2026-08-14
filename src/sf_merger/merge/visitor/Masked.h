#pragma once

#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "merge/visitor/Visitor.h"
#include "mesh/combine.h"
#include "mesh/holes.h"
#include "mesh/merging/SphereProjectionVertexDeduplicate.h"
#include "mesh/merging/VertexMapping.h"
#include "mesh/merging/helpers.h"
#include "mesh/merging/mapping.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "polygon/Polygon.h"
#include "polygon/triangulate.h"
#include "spatial_lookup/Hashmap.h"
#include "utils.h"
#include "atlas/Sheet.h"
#include "atlas/build.h"
#include "atlas/HorizontalStripLayoutPlanner.h"
#include "atlas/LayoutPlanner.h"
#include "atlas/Layout.h"

namespace merge::visitor {

class Masked {
public:
    using Status = octree::NodeStatusOrMissing;
    struct Context {
        MeshMask mask;
        bool has_left_parent;
        bool has_right_parent;
    };
    using Result = merge::Result<Context>;

    explicit Masked(MeshMask mask, octree::Space space) : _mask(mask), _space(space) {}

    Context make_root_context() {
        return Context{
            .mask = this->_mask,
            .has_left_parent = false,
            .has_right_parent = false,
        };
    }

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &id,
        const NodeData<LeftStatus> &left,
        const NodeData<RightStatus> &right,
        const Context &ctx) {
        if constexpr (LeftStatus == Status::Inner || RightStatus == Status::Inner) {
            UNREACHABLE();
        }

        if constexpr (LeftStatus == Status::Missing && RightStatus == Status::Missing) {
            DEBUG_ASSERT(!(ctx.has_left_parent && ctx.has_right_parent));
            if (!ctx.has_left_parent && !ctx.has_right_parent) {
                return Ignore{};
            }
        }

        // If the parent mask is empty, we can just return whatevers on the left
        if constexpr (LeftStatus != Status::Missing) {
            if (ctx.mask.mesh.is_empty()) {
                return Unchanged{Source::Left};
            }
        }

        LOG_TRACE("Clipping mask for {}", id);
        const auto bounds = geometry::pad_bounds_relative(this->_space.get_node_bounds(id), 0.05);
        MeshMask mask(mesh::clip_on_bounds_and_cap(ctx.mask.mesh, bounds));

        // Same when the current mask is empty
        if constexpr (LeftStatus != Status::Missing) {
            if (mask.mesh.is_empty()) {
                return Unchanged{Source::Left};
            }
        }

        // If we have two leaf nodes we can directly merge them.
        if constexpr (LeftStatus == Status::Leaf && RightStatus == Status::Leaf) {
            return this->merge_meshes(left.mesh(), right.mesh(), true, true, mask);
        }

        // If we have a left leaf we either directly return it (after clipping)
        // or merge if right has a non-missing parent
        if constexpr (LeftStatus == Status::Leaf && RightStatus == Status::Missing) {
            if (ctx.has_right_parent) {
                return this->merge_meshes(left.mesh(), right.mesh().value(), true, false, mask);
            }

            // Right node is actually missing, just return the clipped left node
            auto result = clip_on_mask(left.mesh(), mask, false);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{Source::Left};
            } else {
                return Merged{result};
            }
        }

        // If we have a right leaf we either directly return it (after clipping)
        // or merge if left has a non-missing parent
        if constexpr (LeftStatus == Status::Missing && RightStatus == Status::Leaf) {
            if (ctx.has_left_parent) {
                return this->merge_meshes(left.mesh().value(), right.mesh(), false, true, mask);
            }

            // Left node is actually missing, just return the clipped right node
            auto result = clip_on_mask(right.mesh(), mask, true);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{Source::Right};
            } else {
                return Merged{result};
            }
        }

        if constexpr (LeftStatus == Status::Missing && RightStatus == Status::Missing) {
            DEBUG_ASSERT(ctx.has_left_parent || ctx.has_right_parent);
            if (ctx.has_left_parent) {
                auto result = clip_on_mask(left.mesh().value(), mask, false);
                if (result->is_empty()) {
                    return Ignore{};
                } else {
                    return Merged{result};
                }
            }
            if (ctx.has_right_parent) {
                auto result = clip_on_mask(right.mesh().value(), mask, true);
                if (result->is_empty()) {
                    return Ignore{};
                } else {
                    return Merged{result};
                }
            }
        }

        if constexpr (LeftStatus == Status::Virtual || RightStatus == Status::Virtual) {
            return Recurse{
                Context{
                    .mask = mask,
                    .has_left_parent = ctx.has_left_parent || LeftStatus == Status::Leaf,
                    .has_right_parent = ctx.has_right_parent || RightStatus == Status::Leaf}};
        }

        UNREACHABLE();
    }

private:
    Result merge_meshes(
        const SimpleMesh &base_mesh,
        const SimpleMesh &new_mesh,
        const bool can_ref_base_mesh,
        const bool can_ref_new_mesh,
        const MeshMask &mask) {
        LOG_TRACE("Clipping meshes on mask");
        const Cow<const SimpleMesh> new_mesh_clipped = clip_on_mask(new_mesh, mask, true);
        const Cow<const SimpleMesh> base_mesh_clipped = clip_on_mask(base_mesh, mask, false);

        if (base_mesh_clipped.is_ref() && new_mesh_clipped->is_empty()) {
            if (can_ref_base_mesh) {
                return Unchanged{Source::Left};
            } else {
                return Merged{base_mesh_clipped};
            }
        }
        if (new_mesh_clipped.is_ref() && base_mesh_clipped->is_empty()) {
            if (can_ref_new_mesh) {
                return Unchanged{Source::Right};
            } else {
                return Merged{new_mesh_clipped};
            }
        }

        LOG_TRACE("Creating combined mesh");
        const std::array<const std::reference_wrapper<const SimpleMesh>, 2> meshes = {
            base_mesh_clipped,
            new_mesh_clipped,
        };
        std::vector<size_t> vertex_offsets;
        SimpleMesh result_mesh = mesh::combine(meshes, vertex_offsets);
        
        LOG_TRACE("Creating texture atlas");
        const atlas::HorizontalStripLayoutPlanner planner = {};
        const atlas::Layout layout = planner.plan(meshes);
        const atlas::Sheet atlas = atlas::build(meshes, layout);
        result_mesh.texture = std::move(atlas.texture);
        for (size_t mesh_index = 0; mesh_index < meshes.size(); mesh_index++) {
            const SimpleMesh &mesh = meshes[mesh_index];
            const auto vertex_offset = vertex_offsets[mesh_index];
            for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
                result_mesh.uvs[vertex_offset + vertex_index] = atlas.uvs[mesh_index][vertex_index];
            }
        }

        mesh::validate(result_mesh);

        return Merged{result_mesh};

        /*
        TODO: Advanced merging
        const auto base_mesh_bounds = calculate_bounds(base_mesh);
        auto bounds = base_mesh_bounds;
        bounds.expand_by(new_mesh_bounds);
        const glm::dvec3 tangent_point = bounds.centre();
        const glm::dvec2 radius_range = mask::pad_radius_range(mask::calculate_radius_range(bounds), 2);
        if (mask.has_value()) {
            auto result = mesh::intersection_and_difference(new_mesh, mask->mesh);
            const SimpleMesh new_mesh_in_mask = std::move(result.intersection);
            const SimpleMesh new_mesh_out_mask = std::move(result.difference);
            const MeshMask mask_base_mesh = mask::create_from_mesh(base_mesh, tangent_point, radius_range);
            const SimpleMesh new_mesh_out_mask_clipped = mesh::clip_on_mesh(new_mesh_out_mask, mask_base_mesh.mesh);
            const MeshMask mask_new_mesh_in_mask = mask::create_from_mesh(new_mesh_in_mask, tangent_point, radius_range);
            const SimpleMesh base_mesh_clipped = mesh::clip_on_mesh(base_mesh, mask_new_mesh_in_mask.mesh);
            return mesh::merge::merge_meshes(new_mesh_out_mask_clipped, base_mesh_clipped, new_mesh_in_mask);
        } else {
            const MeshMask mask_new_mesh = mask::create_from_mesh(new_mesh, tangent_point, radius_range);
            const SimpleMesh base_mesh_clipped = mesh::clip_on_mesh(base_mesh, mask_new_mesh.mesh);
            return mesh::merge::merge_meshes(base_mesh_clipped, new_mesh);
        }
        */
    }

    MeshMask _mask = {};
    octree::Space _space;
};

static_assert(Visitor<Masked>);

} // namespace merge::visitor
