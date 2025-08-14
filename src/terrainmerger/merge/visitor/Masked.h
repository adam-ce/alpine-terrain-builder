#pragma once

#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "merge/visitor/Base.h"
#include "mesh/merge.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "utils.h"

namespace merge::visitor {
namespace {
inline Result merge_meshes(
    const SimpleMesh &base_mesh,
    const SimpleMesh &new_mesh,
    const MeshMask &mask) {
    
    const Cow<const SimpleMesh> new_mesh_clipped = clip_on_mask(new_mesh, mask, true);
    const Cow<const SimpleMesh> base_mesh_clipped = clip_on_mask(base_mesh, mask, false);
    
    if (base_mesh_clipped.is_ref() && new_mesh_clipped->is_empty()) {
        return Unchanged{.is_left = true};
    }
    if (new_mesh_clipped.is_ref() && base_mesh_clipped->is_empty()) {
        return Unchanged{.is_left = false};
    }

    const SimpleMesh merged_mesh = mesh::merge(base_mesh_clipped, new_mesh_clipped);
    return Merged{merged_mesh};

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
} // namespace

class Masked : public Base {
public:
    using Status = octree::NodeStatusOrMissing;

    explicit Masked(MeshMask mask, octree::Space space) : _space(space) {
        this->_masks[octree::Id::root()] = mask;
    }

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &id,
        NodeData<LeftStatus> &left,
        NodeData<RightStatus> &right) {
        if constexpr (left.status() == Status::Missing && right.status() == Status::Missing) {
            return Ignore{};
        }

        if constexpr (left.status() == Status::Leaf) {
            const MeshMask &mask = this->mask_for_node(id);
            auto result = clip_on_mask(left.mesh(), mask, false);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{.is_left = true};
            } else {
                // TODO: ensure move here
                return Merged{result};
            }
        }

        if constexpr (right.status() == Status::Leaf) {
            const MeshMask &mask = this->mask_for_node(id);
            auto result = clip_on_mask(right.mesh(), mask, true);
            if (result->is_empty()) {
                return Ignore{};
            } else if (result.is_ref()) {
                return Unchanged{.is_left = false};
            } else {
                return Merged{result};
            }
        }

        if constexpr (left.status() == Status::Leaf && right.status() == Status::Leaf) {
            const MeshMask &mask = this->mask_for_node(id);
            return merge_meshes(right.mesh(), left.mesh(), mask);
        }

        return Recurse{};
    }

private:
    std::unordered_map<octree::Id, MeshMask> _masks = {};
    octree::Space _space;

    const MeshMask& mask_for_node(const octree::Id id) {
        // TODO: evict old masks or rewrite such that merge.h handles passing in the masks
        // we return in Recurse{}

        // Try to find cached mask first
        if (auto it = this->_masks.find(id); it != this->_masks.end()) {
            return it->second;
        }

        // Recursively get parent mask (root always exists)
        const octree::Id parent_id = id.parent().value();
        const MeshMask &parent_mask = this->mask_for_node(parent_id);

        // Clip mask based on parent
        const auto bounds = pad_bounds(this->_space.get_node_bounds(id), 1.1);
        LOG_TRACE("Clipping mask for {}", id);
        auto [it, inserted] = this->_masks.try_emplace(
            id,
            mesh::clip_on_bounds_and_cap(parent_mask.mesh, bounds)
        );

        return it->second;
    }
};
} // namespace merge::visitor
