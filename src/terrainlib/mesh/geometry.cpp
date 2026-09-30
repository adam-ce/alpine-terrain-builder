/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/geometry.h"
#include "optional_utils.h"

namespace mesh {
    
std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size) {
    const auto &triangles = mesh.triangles;
    const auto &positions = mesh.positions;
    const size_t num_triangles = triangles.size();

    if (num_triangles == 0) {
        return std::nullopt;
    }

    const size_t triangle_sample_size = std::min((sample_size + 2) / 3, mesh.face_count());
    const size_t stride = std::max<size_t>(1, num_triangles / triangle_sample_size);

    double total_length = 0.0;

    // Use a small offset to avoid sampling only the first part of the mesh
    const size_t offset = (num_triangles / 7) % num_triangles;

    for (size_t i = 0; i < triangle_sample_size; i++) {
        const auto &triangle = triangles[(offset + i * stride) % num_triangles];

        const glm::dvec3 &a = positions[triangle.x];
        const glm::dvec3 &b = positions[triangle.y];
        const glm::dvec3 &c = positions[triangle.z];

        const double ab = glm::distance(a, b);
        const double bc = glm::distance(b, c);
        const double ca = glm::distance(c, a);

        total_length += ab + bc + ca;
    }

    return total_length / (triangle_sample_size * 3);
}

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length_sq = 0.0;
    for (const auto &triangle : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[triangle.x];
        const glm::dvec3 &b = mesh.positions[triangle.y];
        const glm::dvec3 &c = mesh.positions[triangle.z];

        const double ab_sq = glm::distance2(a, b);
        const double bc_sq = glm::distance2(b, c);
        const double ca_sq = glm::distance2(c, a);

        max_length_sq = std::max({
            max_length_sq,
            ab_sq,
            bc_sq,
            ca_sq,
        });
    }
    return max_length_sq;
}

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double min_length_sq = std::numeric_limits<double>::max();
    for (const auto &triangle : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[triangle.x];
        const glm::dvec3 &b = mesh.positions[triangle.y];
        const glm::dvec3 &c = mesh.positions[triangle.z];

        const double ab_sq = glm::distance2(a, b);
        const double bc_sq = glm::distance2(b, c);
        const double ca_sq = glm::distance2(c, a);

        min_length_sq = std::min({
            min_length_sq,
            ab_sq,
            bc_sq,
            ca_sq,
        });
    }
    return min_length_sq;
}

std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh) {
    return map(calculate_max_edge_length_squared(mesh), [](const double max_len_sq) { return std::sqrt(max_len_sq); });
}

std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh) {
    return map(calculate_min_edge_length_squared(mesh), [](const double min_len_sq) { return std::sqrt(min_len_sq); });
}

}
