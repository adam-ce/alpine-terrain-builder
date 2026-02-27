#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"

template <glm::length_t n_dims, typename T>
void reindex_mesh(mesh::Simple_<n_dims, T> &mesh) {
    struct Entry {
        uint32_t new_index;
        uint32_t inv_index;
    };

    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    const Entry invalid_entry = Entry{invalid_index, invalid_index};
    std::vector<Entry> index_map(mesh.positions.size(), invalid_entry);

    // Adjust triangles
    uint32_t next_new_index = 0;
    for (auto &triangle : mesh.triangles) {
        for (uint32_t i = 0; i < 3; i++) {
            Entry &entry = index_map[triangle[i]];
            if (entry.new_index == invalid_index) {
                // Vertex newly encountered
                entry.new_index = triangle[i] = next_new_index;
                next_new_index += 1;
            } else {
                // Vertex already encountered
                triangle[i] = entry.new_index;
            }
        }
    }
    const uint32_t new_vertex_count = next_new_index;

    // Add the inverse index
    for (uint32_t old_index = 0; old_index < index_map.size(); old_index++) {
        Entry &entry = index_map[old_index];
        if (entry.new_index == invalid_index) {
            // This vertex was not used in any triangle
            continue;
        }
        index_map[entry.new_index].inv_index = old_index;
    }

    // Adjust vertices
    for (uint32_t old_index = 0; old_index < new_vertex_count; old_index++) {
        const Entry entry = index_map[old_index];
        std::swap(mesh.positions[old_index], mesh.positions[entry.inv_index]);
        if (mesh.has_uvs()) {
            std::swap(mesh.uvs[old_index], mesh.uvs[entry.inv_index]);
        }
        index_map[entry.inv_index].new_index = entry.new_index;

        if (entry.new_index != invalid_index) {
            index_map[entry.new_index].inv_index = entry.inv_index;
        }
    }

    // Remove unused vertices
    mesh.positions.resize(new_vertex_count);
    if (mesh.has_uvs()) {
        mesh.uvs.resize(new_vertex_count);
    }
}

template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex_mesh(const mesh::Simple_<n_dims, T> &mesh) {
    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(mesh.face_count());
    std::vector<glm::dvec3> new_positions;
    new_positions.reserve(mesh.vertex_count());
    std::vector<glm::dvec2> new_uvs;
    if (mesh.has_uvs()) {
        new_uvs.reserve(mesh.vertex_count());
    }

    const uint32_t invalid_index = static_cast<uint32_t>(-1);
    std::vector<uint32_t> index_map(mesh.positions.size(), invalid_index);

    for (const auto &triangle : mesh.triangles) {
        glm::uvec3 new_triangle_indices;
        for (uint8_t i = 0; i < 3; i++) {
            const uint32_t old_index = triangle[i];
            if (index_map[old_index] == invalid_index) {
                // Vertex newly encountered
                const uint32_t new_index = new_positions.size();
                new_positions.push_back(mesh.positions[old_index]);
                if (mesh.has_uvs()) {
                    new_uvs.push_back(mesh.uvs[old_index]);
                }
                new_triangle_indices[i] = new_index;
                index_map[old_index] = new_index;
            } else {
                // Vertex already encountered
                new_triangle_indices[i] = index_map[old_index];
            }
        }
        new_triangles.push_back(new_triangle_indices);
    }

    SimpleMesh new_mesh(new_triangles, new_positions);
    if (mesh.has_uvs()) {
        new_mesh.uvs = std::move(new_uvs);
    }
    new_mesh.texture = mesh.texture;
    return new_mesh;
}
