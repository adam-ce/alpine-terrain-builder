#pragma once

#include <vector>
#include <array>
#include <algorithm>

#include <glm/common.hpp>

#include "mesh/SimpleMesh.h"

using TriangleSoup = std::vector<std::array<glm::dvec3, 3>>;

inline TriangleSoup to_triangle_soup(const SimpleMesh &mesh) {
    TriangleSoup soup;
    soup.reserve(mesh.triangles.size());
    for (const glm::uvec3 &indices : mesh.triangles) {
        soup.push_back({mesh.positions[indices[0]],
                        mesh.positions[indices[1]],
                        mesh.positions[indices[2]]});
    }
    return soup;
}

inline void sort_triangle_soup(TriangleSoup &soup) {
    for (auto &triangle : soup) {
        // Normalize triangle orientation by rotating so smallest vertex is first
        uint8_t min_index = 0;
        glm::dvec3 min_vertex = triangle[min_index];
        for (uint8_t i = 1; i < 3; i++) {
            const auto &current_vertex = triangle[i];
            if (current_vertex.x < min_vertex.x ||
                (current_vertex.x == min_vertex.x &&
                 (current_vertex.y < min_vertex.y ||
                  (current_vertex.y == min_vertex.y && current_vertex.z < min_vertex.z)))) {
                min_index = i;
                min_vertex = current_vertex;
            }
        }

        // Rotate to put the smallest vertex first
        if (min_index == 1) {
            triangle = {triangle[1], triangle[2], triangle[0]};
        } else if (min_index == 2) {
            triangle = {triangle[2], triangle[0], triangle[1]};
        }
    }

    // Sort the list of triangles
    std::sort(soup.begin(), soup.end(), [](const auto &a, const auto &b) {
        for (uint8_t i = 0; i < 3; i++) {
            if (a[i].x != b[i].x) {
                return a[i].x < b[i].x;
            }
            if (a[i].y != b[i].y) {
                return a[i].y < b[i].y;
            }
            if (a[i].z != b[i].z) {
                return a[i].z < b[i].z;
            }
        }
        return false; // triangles are equal
    });
}

inline TriangleSoup to_sorted_triangle_soup(const SimpleMesh &mesh) {
    auto soup = to_triangle_soup(mesh);
    sort_triangle_soup(soup);
    return soup;
}
