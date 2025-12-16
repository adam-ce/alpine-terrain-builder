#pragma once

#include <span>
#include <vector>

#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>

#include "mesh/SimpleMesh.h"
#include "cluster.h"

// Normalize a set of 3D positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
inline void to_approximate_normalized(std::span<const glm::dvec3> positions, 
                                      std::vector<glm::vec3> &approx,
                                      radix::geometry::Aabb3d *out_bounds = nullptr) {
    // compute bounds
    const radix::geometry::Aabb3d bounds = radix::geometry::find_bounds(positions);
    const glm::dvec3 center = bounds.centre();
    const glm::dvec3 extents = bounds.size() / 2.0;
    const double max_extents = glm::compMax(extents);

    if (out_bounds) {
        *out_bounds = bounds;
    }

    // normalize based on aabb
    approx.clear();
    approx.reserve(positions.size());
    for (const auto &p : positions) {
        const glm::dvec3 rel = (p - center) / max_extents;
        approx.push_back(glm::vec3(rel));
    }
}

// Normalize a set of 3D positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
inline std::vector<glm::vec3> to_approximate_normalized(std::span<const glm::dvec3> positions,
                                                        radix::geometry::Aabb3d *out_bounds = nullptr) {
    std::vector<glm::vec3> approx;
    to_approximate_normalized(positions, approx, out_bounds);
    return approx;
}

inline mesh::Simple materialize_cluster(const Cluster& cluster, const std::span<const glm::dvec3> positions) {
    mesh::Simple mesh;
    mesh.positions.reserve(cluster.vertex_indices.size());
    for (const uint32_t vertex_index : cluster.vertex_indices) {
        mesh.positions.push_back(positions[vertex_index]);
    }
    mesh.triangles = cluster.local_triangles;
    return mesh;
}

using Rgb = glm::tvec3<uint8_t>;
inline std::vector<Rgb> generate_distinct_colors(const size_t count) {
    std::vector<Rgb> colors(count);

    for (size_t i = 0; i < count; i++) {
        const uint8_t r = (i * 73) % 256;
        const uint8_t g = (i * 151) % 256;
        const uint8_t b = (i * 197) % 256;
        colors[i] = Rgb(r, g, b);
    }

    return colors;
}

inline mesh::Simple clustering_to_mesh(const Clustering &clustering) {
    const size_t cluster_count = clustering.clusters.size();
    if (cluster_count == 0) {
        return {};
    }

    // Compute total vertices and triangles
    size_t total_vertices = 0;
    size_t total_triangles = 0;
    for (const Cluster &cluster : clustering.clusters) {
        total_vertices += cluster.vertex_indices.size();
        total_triangles += cluster.local_triangles.size();
    }

    // Preallocate mesh buffers
    mesh::Simple mesh;
    mesh.positions.reserve(total_vertices);
    mesh.uvs.reserve(total_vertices);
    mesh.triangles.reserve(total_triangles);

    // Prepare texture
    const std::vector<Rgb> cluster_colors = generate_distinct_colors(cluster_count);
    const size_t texture_size = static_cast<size_t>(std::ceil(std::sqrt(cluster_count)));
    const size_t actual_texture_size = texture_size * 3;
    cv::Mat texture = cv::Mat(actual_texture_size, actual_texture_size, CV_8UC3, cv::Scalar(0, 0, 0));

    // Append vertices and assign UVs
    for (size_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t base_vertex = mesh.vertex_count();

        // Compute UV coordinates for this cluster
        const size_t block_row = (cluster_index / texture_size) * 3;
        const size_t block_col = (cluster_index % texture_size) * 3;
        const double u = (block_col + 1.5) / actual_texture_size;
        const double v = (block_row + 1.5) / actual_texture_size;

        // Append vertices and UVs
        for (const uint32_t vertex_index : cluster.vertex_indices) {
            mesh.positions.push_back(clustering.positions[vertex_index]);
            mesh.uvs.push_back(glm::dvec2(u, v));
        }

        // Fill cluster color in texture
        const Rgb &color = cluster_colors[cluster_index];
        texture.at<cv::Vec3b>(block_row + 1, block_col + 1) = {color.z, color.y, color.x};

        // Append triangles
        for (const glm::uvec3 &local_triangle : cluster.local_triangles) {
            mesh.triangles.push_back(local_triangle + base_vertex);
        }
    }

    mesh.texture = texture;

    return mesh;
}
