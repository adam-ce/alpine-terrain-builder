#pragma once

#include <expected>

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "log.h"
#include "mask.h"
#include "octree/Id.h"
#include "mesh/storage.h"
#include "octree/Space.h"
#include "utils.h"
#include "containers/Cow.h"
#include "Error.h"
#include "sf/finalize_storage.h"
#include "sf/validate_index.h"

struct Context {
    const mesh::storage::IndexedStorage& input;
    mesh::storage::Storage& output;
    const octree::Space space;
    const bool keep_inside;
};

Expected<void> cut_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask);

inline Expected<void> cut_leaf_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    DEBUG_ASSERT(DEBUG_ASSERT_VAL(
        ctx.input.index().is(store::NodeStatus::Leaf, id)).value());

    const SimpleMesh mesh = DEBUG_ASSERT_VAL(ctx.input.load(id)).value();
    LOG_TRACE("Cutting mesh at {} using mask with {} vertices and {} triangles",
        id, mask.vertex_count(), mask.face_count());
    const Cow<const SimpleMesh> clipped = clip_on_mask(mesh, mask, ctx.keep_inside);
    if (clipped.is_ref()) {
        LOG_TRACE("Mesh was fully inside the mask");
        auto copy_result = ctx.output.copy_from(id, ctx.input);
        if (!copy_result.has_value()) {
            return copy_result;
        }
    } else {
        const SimpleMesh &clipped_mesh = clipped;
        if (!clipped_mesh.is_empty()) {
            LOG_TRACE("Mesh was clipped from {} vertices and {} triangles to {} vertices and {} triangles",
                mesh.vertex_count(), mesh.face_count(), clipped_mesh.vertex_count(), clipped_mesh.face_count());
            auto save_result = ctx.output.save(id, clipped_mesh);
            if (!save_result.has_value()) {
                return save_result;
            }
        } else {
            LOG_TRACE("Mesh was fully outside the mask");
        }
    }
    return {};
}

inline Expected<void> cut_virtual_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    DEBUG_ASSERT(DEBUG_ASSERT_VAL(
        ctx.input.index().is(store::NodeStatus::Virtual, id)).value());
    DEBUG_ASSERT(id.has_children());

    const auto children = id.children().value();
    for (const octree::Id& child_id : children) {
        // TODO: split mask based on inner planes instead
        const auto child_bounds = geometry::pad_bounds_relative(ctx.space.get_node_bounds(child_id), 0.125);
        LOG_TRACE("Clipping mask for {}", child_id);
        const MeshMask child_mask = clip_mask_on_bounds(mask, child_bounds);
        auto child_result = cut_node(ctx, child_id, child_mask);
        if (!child_result.has_value()) {
            return child_result;
        }
    }
    return {};
}

inline Expected<void> cut_node(
    Context& ctx,
    const octree::Id& id,
    const MeshMask& mask
) {
    if (ctx.keep_inside && mask.is_empty()) {
        LOG_TRACE("Mesh was fully outside the mask");
        return {};
    }

    /*
    Uncomment to output masks:
    const auto path = ctx.output.path_for(id);
    const auto new_path = path.parent_path() /
                          (path.stem().string() + "-mask" + path.extension().string());
    mesh::io::save_to_path(mask.mesh, new_path);
    */

    const auto status_result = ctx.input.index().get(id);
    DEBUG_ASSERT(status_result.has_value());
    if (!status_result->has_value()) {
        return {};
    }

    const store::NodeStatus status = status_result->value();
    switch (status) {
    case store::NodeStatus::Virtual:
        return cut_virtual_node(ctx, id, mask);
    case store::NodeStatus::Leaf:
        return cut_leaf_node(ctx, id, mask);
    default:
        UNREACHABLE();
        return {};
    }
    return {};
}

inline Expected<void> cut_dataset(
    const mesh::storage::IndexedStorage &input,
    const MeshMask& mask,
    mesh::storage::Storage &output,
    const bool keep_inside) {
    auto validation = sf::validate_index(input.index());
    if (!validation.has_value()) {
        return validation;
    }
    Context ctx(input, output, octree::Space::earth(), keep_inside);
    auto cut_result = cut_node(ctx, octree::Id::root(), mask);
    if (!cut_result.has_value()) {
        return cut_result;
    }
    return sf::finalize_storage(output);
}

inline Expected<void> cut_dataset(
    const mesh::storage::IndexedStorage &input_dataset,
    const MeshMask& mask,
    const std::filesystem::path &output_path,
    const bool keep_inside) {
    LOG_TRACE("Creating output dataset at {}", output_path);
    std::filesystem::create_directories(output_path);

    mesh::storage::OpenOptions options;
    options.default_mapping = octree::store_layout::level_and_coordinate_directories();
    options.preferred_extension = ".glb";
    auto output_result = mesh::storage::open_folder_indexed(output_path, std::move(options));
    if (!output_result.has_value()) {
        return Error::propagate(std::move(output_result));
    }
    mesh::storage::IndexedStorage output_dataset = std::move(output_result.value());
    if (!output_dataset.index().empty()) {
        return Error::fail(Error::Code::AlreadyExists, "output dataset already contains nodes: " + output_path.string());
    }

    return cut_dataset(input_dataset, mask, output_dataset, keep_inside);
}
