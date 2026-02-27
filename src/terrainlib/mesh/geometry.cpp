#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/geometry.h"

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
        const auto &tri = triangles[(offset + i * stride) % num_triangles];

        const glm::dvec3 &a = positions[tri.x];
        const glm::dvec3 &b = positions[tri.y];
        const glm::dvec3 &c = positions[tri.z];

        total_length += glm::distance(a, b) + glm::distance(b, c) + glm::distance(c, a);
    }

    return total_length / triangle_sample_size;
}

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length = 0.0;
    for (const auto &tri : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        const double ab = glm::distance2(a, b);
        const double bc = glm::distance2(b, c);
        const double ca = glm::distance2(c, a);

        max_length = std::max({ab, bc, ca, max_length});
    }
    return max_length;
}

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length = 0.0;
    for (const auto &tri : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[tri.x];
        const glm::dvec3 &b = mesh.positions[tri.y];
        const glm::dvec3 &c = mesh.positions[tri.z];

        const double ab = glm::distance2(a, b);
        const double bc = glm::distance2(b, c);
        const double ca = glm::distance2(c, a);

        max_length = std::min({ab, bc, ca, max_length});
    }
    return max_length;
}

std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh) {
    auto length_sq_opt = calculate_max_edge_length_squared(mesh);
    if (length_sq_opt.has_value()) {
        return std::sqrt(length_sq_opt.value());
    } else {
        return std::nullopt;
    }
}

std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh) {
    auto length_sq_opt = calculate_min_edge_length_squared(mesh);
    if (length_sq_opt.has_value()) {
        return std::sqrt(length_sq_opt.value());
    } else {
        return std::nullopt;
    }
}