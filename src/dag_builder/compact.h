#pragma once

#include <vector>

#include "cluster.h"

void compact_cluster(Cluster &cluster) {
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
    constexpr uint32_t invalid_remap = -1;
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
