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

    double min_length_sq = 0.0;
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
