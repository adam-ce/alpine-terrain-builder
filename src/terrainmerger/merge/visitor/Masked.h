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
    const Cow<const SimpleMesh> new_mesh_clipped = clip_on_mask(new_mesh, mask);
    const Cow<const SimpleMesh> base_mesh_clipped = clip_on_mask(base_mesh, mask);
    if (base_mesh_clipped.is_ref() && new_mesh_clipped->is_empty()) {
        return Unchanged{true};
    }
    if (new_mesh_clipped.is_ref() && base_mesh_clipped->is_empty()) {
        return Unchanged{false};
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

    explicit Masked(MeshMask mask) : mask(mask) {}

    MeshMask mask;

    template <Status LeftStatus, Status RightStatus>
    Result visit(
        const octree::Id &,
        NodeData<LeftStatus> &left,
        NodeData<RightStatus> &right) {

        if constexpr (right.status() == Status::Missing) {
            return Unchanged{false};
        }

        if constexpr (left.status() == Status::Missing) {
            if constexpr (right.status() == Status::Leaf) {
                auto result = clip_on_mask(right.mesh(), this->mask);
                if (result.is_ref()) {
                    return Unchanged{true};
                } else {
                    return Merged{result};
                }
            } else {
                return Recurse{};
            }
        }

        if constexpr (left.status() == Status::Leaf && right.status() == Status::Leaf) {
            return merge_meshes(right.mesh(), left.mesh(), this->mask);
        }

        return Recurse{};
    }
};
} // namespace merge::visitor
