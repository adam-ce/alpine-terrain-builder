#pragma once

#include <vector>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

#include "cluster.h"
#include "enumerate.h"
#include "slice.h"
#include "validate.h"

inline void compact_cluster_inplace(Cluster &cluster) {
    const size_t vertex_count = cluster.vertex_count();
    if (vertex_count == 0) {
        return;
    }

    // Mark used vertices
    std::vector<bool> used(vertex_count, false);
    for (const auto &triangle : cluster.local_triangles) {
        used[triangle.x] = true;
        used[triangle.y] = true;
        used[triangle.z] = true;
    }

    // Build remap
    constexpr uint32_t invalid_remap = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> remap(vertex_count, invalid_remap);
    std::vector<uint32_t> new_vertex_indices;
    std::vector<glm::dvec2> new_uvs;

    new_vertex_indices.reserve(vertex_count);
    if (cluster.has_uvs()) {
        new_uvs.reserve(vertex_count);
    }

    for (uint32_t i = 0; i < vertex_count; i++) {
        if (used[i]) {
            remap[i] = new_vertex_indices.size();
            new_vertex_indices.push_back(cluster.vertex_indices[i]);
            if (cluster.has_uvs()) {
                new_uvs.push_back(cluster.uvs[i]);
            }
        }
    }

    // Remap triangles
    for (auto &triangle : cluster.local_triangles) {
        triangle.x = remap[triangle.x];
        triangle.y = remap[triangle.y];
        triangle.z = remap[triangle.z];
    }

    // Replace data
    cluster.vertex_indices = std::move(new_vertex_indices);
    cluster.uvs = std::move(new_uvs);
}

inline void remove_unused_vertices_inplace(Clustering &clustering) {
    const uint32_t vertex_count = clustering.vertex_count();
    constexpr uint32_t invalid_vertex = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> vertex_remap(vertex_count, invalid_vertex);
    uint32_t next_index = 0;
    for (Cluster &cluster : clustering.clusters) {
        for (uint32_t &vertex_index : cluster.vertex_indices) {
            uint32_t &new_index = vertex_remap[vertex_index];
            if (new_index == invalid_vertex) {
                new_index = next_index;
                next_index++;
            }
            vertex_index = new_index;
        }
    }

    const uint32_t new_vertex_count = next_index;
    std::vector<glm::dvec3> new_positions(new_vertex_count);
    for (const auto [old_index, new_index] : enumerate(vertex_remap)) {
        if (new_index == invalid_vertex) {
            continue;
        }
        new_positions[new_index] = clustering.positions[old_index];
    }
    clustering.positions = new_positions;

    validate(clustering);
}
inline Clustering remove_unused_vertices(const Clustering &clustering) {
    Clustering copy = clustering;
    remove_unused_vertices_inplace(copy);
    return copy;
}
