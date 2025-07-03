#pragma once

#include <span>
#include <array>

#include "mesh/SimpleMesh.h"
#include "mesh/utils.h"
#include "earth.h"
#include "log.h"
#include "mask.h"
#include "mesh/boolean.h"
#include "mesh/clip.h"
#include "mesh/merge.h"
#include "mesh/validate.h"
#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "octree/Space.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/open.h"
#include "octree/traverse.h"

using MeshMask = mask::MeshMask;

inline SimpleMesh combine_meshes(
    const SimpleMesh& a,
    const SimpleMesh& b
) {
    if (a.is_empty()) {
        return b;
    }
    if (b.is_empty()) {
        return a;
    }

    SimpleMesh combined_mesh;
    combined_mesh.positions.reserve(a.vertex_count() + b.vertex_count());
    combined_mesh.uvs.reserve(a.vertex_count() + b.vertex_count());
    combined_mesh.triangles.reserve(a.face_count() + b.face_count());
    combined_mesh.positions.insert(combined_mesh.positions.end(), a.positions.begin(), a.positions.end());
    combined_mesh.positions.insert(combined_mesh.positions.end(), b.positions.begin(), b.positions.end());
    combined_mesh.uvs.insert(combined_mesh.uvs.end(), a.uvs.begin(), a.uvs.end());
    combined_mesh.uvs.insert(combined_mesh.uvs.end(), b.uvs.begin(), b.uvs.end());
    combined_mesh.triangles.insert(combined_mesh.triangles.end(), a.triangles.begin(), a.triangles.end());
    const size_t offset = a.vertex_count();
    for (const auto& triangle : b.triangles) {
        combined_mesh.triangles.emplace_back(triangle.x + offset, triangle.y + offset, triangle.z + offset);
    }
}

inline SimpleMesh merge_meshes(
    const SimpleMesh& base_mesh, // base mesh
    const SimpleMesh& new_mesh, // priority mesh
    const std::optional<mask::MeshMask> mask
) {
    // TODO: dont convert to SimpleMesh all the time
    if (mask.has_value()) {
        const SimpleMesh new_mesh_clipped = mesh::clip_on_mesh(new_mesh, mask->mesh);
        const SimpleMesh base_mesh_clipped = mesh::clip_on_mesh(base_mesh, mask->mesh);

        return combine_meshes(base_mesh_clipped, new_mesh_clipped);
        // return mesh::merge::merge_meshes(base_mesh_clipped, new_mesh_clipped);
    } else {
        // If one mesh is empty, return the other
        // We can only do this if there is no mask, otherwise we need to at least clip the meshes
        if (base_mesh.is_empty()) {
            return new_mesh;
        }
        if (new_mesh.is_empty()) {
            return base_mesh;
        }

        // TODO: this doesnt really work well if there are intersections
        return mesh::merge::merge_meshes(base_mesh, new_mesh);
    }

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

inline std::optional<std::array<octree::IdAndNode, 8>> split_mesh_into_children(const octree::IdAndNode& node) {
    if (!node.id.has_children()) {
        return std::nullopt;
    }

    const auto space = octree::Space::earth(); // TODO: store in storage

    const auto children = node.id.children().value();
    std::array<octree::IdAndNode, children.size()> child_nodes;
    for (size_t i = 0; i < children.size(); i++) {
        const auto& child_id = children[i];
        const auto child_bounds = space.get_node_bounds(child_id);
        const_cast<SimpleMesh&>(node.node).uvs.clear(); // TODO: remove this when we can handle uvs during clipping
        const auto child_mesh = mesh::clip_on_bounds(node.node, child_bounds);
        mesh::validate(child_mesh);
        child_nodes[i] = {child_id, std::move(child_mesh)};
    }
    return child_nodes;
}

struct MergeState {
    octree::IndexedStorage& output;
    const octree::IndexedStorage& input;
    const std::optional<MeshMask> &input_mask;
};

inline void merge_leaves(
    MergeState& state,
    const octree::Id& id
) {
    LOG_TRACE("Merging leaves for id: {}", id);

    DEBUG_ASSERT(state.output.index().is(octree::NodeStatus::Leaf, id));
    DEBUG_ASSERT(state.input.index().is(octree::NodeStatus::Leaf, id));
    const auto output_mesh = state.output.read_node(id).value();
    const auto input_mesh = state.input.read_node(id).value();

    mesh::validate(output_mesh);
    mesh::validate(input_mesh);

    SimpleMesh merged_mesh = merge_meshes(
        output_mesh,
        input_mesh,
        state.input_mask
    );
    state.output.write_node(id, merged_mesh);
}

inline void merge_with_leaf(
    MergeState &state,
    const octree::Id &id,
    const SimpleMesh &leaf_mesh,
    bool leaf_is_output, // true if leaf is from output, false if from input
    bool leaf_is_clipped // true if the leaf mesh was generated by clipping
) {
    LOG_TRACE("Merging leaf from {} for id: {}",
              leaf_is_output ? "output" : "input",
              id);

    if (leaf_is_output) {
        if (leaf_is_clipped) {
            DEBUG_ASSERT(state.output.index().is_absent(id));
        } else {
            DEBUG_ASSERT(state.output.index().is(octree::NodeStatus::Leaf, id));
        }
        DEBUG_ASSERT(state.input.index().is(octree::NodeStatus::Virtual, id));
    } else {
        if (leaf_is_clipped) {
            DEBUG_ASSERT(state.input.index().is_absent(id));
        } else {
            DEBUG_ASSERT(state.input.index().is(octree::NodeStatus::Leaf, id));
        }
        DEBUG_ASSERT(state.output.index().is(octree::NodeStatus::Virtual, id));
    }
    DEBUG_ASSERT(id.has_children());

    // Split the leaf mesh into children
    const auto leaf_children = split_mesh_into_children({id, leaf_mesh});
    DEBUG_ASSERT(leaf_children.has_value());

    // Merge each child
    for (const auto& child : *leaf_children) {
        const auto& child_id = child.id;
        const auto& child_mesh = child.node;

        // Load other child mesh from the other dataset
        const auto other_status_opt = leaf_is_output
            ? state.input.index().get(child_id)
            : state.output.index().get(child_id);
        if (!other_status_opt.has_value()) {
            // TODO: dont generate meshes for non-existing input nodes
            // If the other child does not exist, we can just write the clipped mesh from the leaf
            state.output.write_node(child_id, child_mesh);
            continue;
        }
        const auto other_status = other_status_opt.value();
        DEBUG_ASSERT(other_status != octree::NodeStatus::Inner);

        if (other_status == octree::NodeStatus::Virtual) {
            // If the other child is virtual, we recurse further
            merge_with_leaf(state, child_id, child_mesh, leaf_is_output, true);
        } else if (other_status == octree::NodeStatus::Leaf) {
            // If the other child is a leaf, we can merge it directly
            SimpleMesh merged_mesh;
            if (leaf_is_output) {
                // If the leaf is from output, we read from input
                const auto other_mesh = state.input.read_node(child_id).value();
                merged_mesh = merge_meshes(child_mesh, other_mesh, state.input_mask);
            } else {
                // If the leaf is from input, we read from output
                const auto other_mesh = state.output.read_node(child_id).value();
                merged_mesh = merge_meshes(other_mesh, child_mesh, state.input_mask);
            }
            state.output.write_node(child_id, merged_mesh);
        } else {
            UNREACHABLE();
        }
    }
}

inline void copy_subtree_to_output(
    MergeState& state,
    const octree::Id& id
) {
    LOG_TRACE("Copying subtree to output for id: {}", id);

    DEBUG_ASSERT(state.input.has_node(id));
    DEBUG_ASSERT(!state.output.has_node(id));

    octree::traverse(
        state.input.index(), 
        [&](const octree::Id& child_id, const octree::NodeStatus& status) {
            if (status == octree::NodeStatus::Virtual) {
                return;
            }
            DEBUG_ASSERT(status == octree::NodeStatus::Leaf);
            
            const auto child_mesh = state.input.read_node(child_id).value();
            state.output.write_node(child_id, child_mesh);
        },
        [](const octree::Id &) { return true; },
        id
    );
}

inline void merge_node(
    MergeState& state,
    const octree::Id& id
) {
    LOG_TRACE("Merging nodes for id: {}", id);

    const auto output_state_opt = state.output.index().get(id);
    const auto input_state_opt = state.input.index().get(id);

    if (!input_state_opt.has_value()) {
        // If the input does not have the node, we can skip it
        return;
    }
    if (!output_state_opt.has_value()) {
        // If the output does not have the node, we can copy it
        copy_subtree_to_output(state, id);
        return;
    }

    const auto output_status = output_state_opt.value();
    const auto input_status = input_state_opt.value();

    DEBUG_ASSERT(output_status != octree::NodeStatus::Inner);
    DEBUG_ASSERT(input_status != octree::NodeStatus::Inner);

    if (output_status == octree::NodeStatus::Leaf && input_status == octree::NodeStatus::Leaf) {
        // If both nodes are leaves, we can merge them directly
        merge_leaves(state, id);
    } else if (output_status == octree::NodeStatus::Leaf && input_status == octree::NodeStatus::Virtual) {
        merge_with_leaf(state, id, state.output.read_node(id).value(), true, false);
    } else if (output_status == octree::NodeStatus::Virtual && input_status == octree::NodeStatus::Leaf) {
        merge_with_leaf(state, id, state.input.read_node(id).value(), false, false);
    } else if (output_status == octree::NodeStatus::Virtual && input_status == octree::NodeStatus::Virtual) {
        // If both nodes are virtual, recurse
        DEBUG_ASSERT(id.has_children());
        const auto children = id.children().value();
        for (const auto& child_id : children) {
            merge_node(state, child_id);
        }
    } else {
        UNREACHABLE();
    }
}

inline void merge_datasets(octree::IndexedStorage& output, const octree::IndexedStorage& input) {
    std::optional<mask::MeshMask> mask = std::nullopt;
    const auto mask_path = input.base_path() / "mask.geojson";
    LOG_TRACE("Looking for mask file at {}", mask_path);
    if (std::filesystem::exists(mask_path)) {
        LOG_INFO("Loading mask file from {}", mask_path);
        const glm::dvec2 radius_range = mask::pad_radius_range(earth::radius_range(), 2);
        auto mask_result = mask::load_from_path(mask_path, radius_range);
        if (mask_result.has_value()) {
            mask = std::move(mask_result.value());
            LOG_DEBUG("Loaded mask successfully");
        } else {
            LOG_ERROR("Failed to load mask: {}", mask_result.error().description());
        }
    } else {
        LOG_DEBUG("No mask file found at {}, proceeding without mask", mask_path);
    }

    MergeState state{output, input, mask};
    merge_node(state, octree::Id::root());
    output.save_index();
}

inline void merge_datasets(octree::IndexedStorage& output, const std::span<octree::IndexedStorage> inputs) {
    for (const auto& input : inputs) {
        merge_datasets(output, input);
    }
}
