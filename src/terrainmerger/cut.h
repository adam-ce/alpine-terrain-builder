#pragma once

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "log.h"
#include "mask.h"
#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/open.h"
#include "octree/Space.h"
#include "octree/traverse.h"
#include "utils.h"
#include "Cow.h"

struct Context {
    const octree::IndexedStorage& input;
    octree::Storage& output;
    const octree::Space space;
};

void cut_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask);

inline void cut_leaf_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    DEBUG_ASSERT(ctx.input.index().is(octree::NodeStatus::Leaf, id));

    const SimpleMesh mesh = DEBUG_ASSERT_VAL(ctx.input.read_node(id)).value();
    LOG_TRACE("Cutting mesh at {} using mask with {} vertices and {} triangles", 
        id, mask.mesh.vertex_count(), mask.mesh.face_count());
    const Cow<const SimpleMesh> clipped = clip_on_mask(mesh, mask);
    if (clipped.is_ref()) {
        LOG_TRACE("Mesh was fully inside the mask");
        DEBUG_ASSERT_VAL(ctx.input.copy_node_to(id, ctx.output));
    } else {
        const SimpleMesh &clipped_mesh = clipped;
        if (!clipped_mesh.is_empty()) {
            LOG_TRACE("Mesh was clipped from {} vertices and {} triangles to {} vertices and {} triangles", 
                mesh.vertex_count(), mesh.face_count(), clipped_mesh.vertex_count(), clipped_mesh.face_count());
            DEBUG_ASSERT_VAL(ctx.output.write_node(id, clipped_mesh));
        } else {
            LOG_TRACE("Mesh was fully ouside the mask");
        }
    }
}

namespace {
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds(const radix::geometry::Aabb<n_dims, T> &bounds, const T factor) {
    using Vec = glm::vec<n_dims, T>;
    const Vec center = bounds.centre();
    const Vec half_size = bounds.size() * (factor / T(2));
    const Vec new_min = center - half_size;
    const Vec new_max = center + half_size;
    return radix::geometry::Aabb<n_dims, T>(new_min, new_max);
}
}

inline void cut_virtual_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    DEBUG_ASSERT(ctx.input.index().is(octree::NodeStatus::Virtual, id));
    DEBUG_ASSERT(id.has_children());

    const auto children = id.children().value();
    for (const octree::Id& child_id : children) {
        // TODO: split mask based on inner planes instead
        const auto child_bounds = pad_bounds(ctx.space.get_node_bounds(child_id), 1.25);
        LOG_TRACE("Clipping mask for {}", child_id);
        const auto child_mask = MeshMask(mesh::clip_on_bounds_and_cap(mask.mesh, child_bounds));
        cut_node(ctx, child_id, child_mask);
    }
}

inline void cut_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    if (mask.mesh.is_empty()) {
        return;
    }

    /*
    Uncomment to output masks
    const auto path = ctx.output.get_node_path(id);
    const auto new_path = path.parent_path() /
                          (path.stem().string() + "-mask" + path.extension().string());
    mesh::io::save_to_path(mask.mesh, new_path);
    */

    const auto status_opt = ctx.input.index().get(id);
    if (!status_opt.has_value()) {
        return;
    }

    const octree::NodeStatus status = status_opt.value();
    switch (status) {
    case octree::NodeStatus::Virtual:
        cut_virtual_node(ctx, id, mask);
        break;
    case octree::NodeStatus::Leaf:
        cut_leaf_node(ctx, id, mask);
        break;
    default:
        UNREACHABLE();
        break;
    }
}

inline void cut_dataset(
    const octree::IndexedStorage &input,
    const MeshMask& mask,
    octree::Storage &output) {
    Context ctx(input, output, octree::Space::earth());
    cut_node(ctx, octree::Id::root(), mask);
    output.save_or_create_index();
}

inline void cut_dataset(
    const octree::IndexedStorage &input_dataset,
    const MeshMask& mask,
    const std::filesystem::path &output_path) {
    LOG_TRACE("Creating output dataset at {}", output_path);
    std::filesystem::create_directories(output_path);

    octree::IndexedStorage output_dataset = octree::open_folder_indexed(output_path, octree::OpenOptions(octree::disk::layout::strategy::make_default(), ".glb"));
    if (!output_dataset.index().empty()) {
        LOG_ERROR_AND_EXIT("Output dataset has to be empty.");
    }

    cut_dataset(input_dataset, mask, output_dataset);
}

