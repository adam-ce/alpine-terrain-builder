#pragma once

#include <span>
#include <vector>

#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>

#include "atlas/rect/atlas.h"
#include "cluster.h"
#include "geometry/geometry.h"
#include "mesh/cleanup.h"
#include "mesh/SimpleMesh.h"
#include "mesh/connectivity/manifold.h"
#include "mesh/texture_trim.h"
#include "range_utils.h"
#include "vector_utils.h"
#include "enumerate.h"

inline void collect_cluster_positions(const Cluster &cluster, const std::span<const glm::dvec3> global_positions, std::vector<glm::dvec3> &out_positions) {
    out_positions.clear();
    out_positions.reserve(cluster.vertex_indices.size());
    for (const uint32_t vertex_index : cluster.vertex_indices) {
        DEBUG_ASSERT(vertex_index < global_positions.size());
        out_positions.push_back(global_positions[vertex_index]);
    }
}

inline std::vector<glm::dvec3> collect_cluster_positions(const Cluster &cluster, const std::span<const glm::dvec3> global_positions) {
    std::vector<glm::dvec3> positions;
    collect_cluster_positions(cluster, global_positions, positions);
    return positions;
}

inline radix::geometry::Aabb3d compute_cluster_bounds(const Cluster &cluster, const std::span<const glm::dvec3> global_positions) {
    return radix::geometry::find_bounds(std::span<const glm::dvec3>(collect_cluster_positions(cluster, global_positions)));
}

inline mesh::Simple materialize_cluster(const Cluster& cluster, const std::span<const glm::dvec3> positions) {
    mesh::Simple mesh;
    mesh.positions = collect_cluster_positions(cluster, positions);
    mesh.triangles = cluster.local_triangles;
    return mesh;
}

using Rgb = glm::tvec3<uint8_t>;
inline std::vector<Rgb> generate_colors(const size_t count) {
    std::vector<Rgb> colors(count);

    for (size_t i = 0; i < count; i++) {
        const uint8_t r = (i * 73) % 256;
        const uint8_t g = (i * 151) % 256;
        const uint8_t b = (i * 197) % 256;
        colors[i] = Rgb(r, g, b);
    }

    return colors;
}

inline void make_manifold_inplace(Cluster &cluster) {
    auto duplicate_vertex = [&](const uint32_t old_vertex_index) {
        const uint32_t new_vertex_index = cluster.vertex_indices.size();
        // No need to update cluster_positions here.
        cluster.vertex_indices.push_back(cluster.vertex_indices[old_vertex_index]);
        if (cluster.has_uvs()) {
            cluster.uvs.push_back(cluster.uvs[old_vertex_index]);
        }
        return new_vertex_index;
    };

    mesh::make_manifold(cluster.local_triangles, cluster.vertex_count(), duplicate_vertex);
}
inline void make_manifold_inplace(Clustering &clustering) {
    for (Cluster &cluster : clustering.clusters) {
        make_manifold_inplace(cluster);
    }
}
inline Clustering make_manifold(const Clustering &clustering) {
    Clustering manifold = clustering;
    make_manifold_inplace(manifold);
    return manifold;
}

inline void remove_duplicate_triangles_inplace(Clustering &clustering) {
    std::vector<glm::uvec3> global_triangles;
    for (Cluster &cluster : clustering.clusters) {
        global_triangles.clear();
        global_triangles.reserve(cluster.local_triangles.size());
        for (const glm::uvec3 &triangle : cluster.local_triangles) {
            global_triangles.emplace_back(
                cluster.vertex_indices[triangle.x],
                cluster.vertex_indices[triangle.y],
                cluster.vertex_indices[triangle.z]);
        }

        const std::vector<uint32_t> duplicates = mesh::find_duplicate_triangles_consider_orientation(global_triangles);
        for (const uint32_t index : duplicates | std::views::reverse) {
            erase_by_index(cluster.local_triangles, index);
        }
    }
}

namespace detail {
inline mesh::Simple manifold_clustering_to_mesh(const Clustering &clustering, const bool debug_texture = false) {
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

    const bool any_has_uvs = std::ranges::any_of(clustering.clusters, [](const Cluster &c) {
        return c.is_textured();
    });

    // Preallocate mesh buffers
    mesh::Simple mesh;
    mesh.positions.reserve(total_vertices);
    if (any_has_uvs) {
        mesh.uvs.reserve(total_vertices);
    }
    mesh.triangles.reserve(total_triangles);

    // Append vertices and triangles
    for (size_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        const Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t base_vertex = mesh.vertex_count();

        // Append vertices
        for (size_t i = 0; i < cluster.vertex_indices.size(); i++) {
            const uint32_t vertex_index = cluster.vertex_indices[i];
            mesh.positions.push_back(clustering.positions[vertex_index]);
            if (any_has_uvs) {
                const glm::dvec2 uv = cluster.has_uvs() ? cluster.uvs[i] : glm::dvec2(0);
                mesh.uvs.push_back(uv);
            }
        }

        // Append triangles
        for (const glm::uvec3 &local_triangle : cluster.local_triangles) {
            mesh.triangles.push_back(local_triangle + base_vertex);
        }
    }
    
    DEBUG_ASSERT(mesh::is_manifold(mesh));

    if (debug_texture) {
        mesh.uvs.clear();

        // Prepare texture
        const std::vector<Rgb> cluster_colors = generate_colors(cluster_count);
        const size_t texture_size = static_cast<size_t>(std::ceil(std::sqrt(cluster_count)));
        const size_t actual_texture_size = texture_size * 3;
        cv::Mat texture = cv::Mat(actual_texture_size, actual_texture_size, CV_8UC3, cv::Scalar(0, 0, 0));

        // Append vertices and assign UVs
        for (size_t cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
            const Cluster &cluster = clustering.clusters[cluster_index];

            // Compute UV coordinates for this cluster
            const size_t block_row = (cluster_index / texture_size) * 3;
            const size_t block_col = (cluster_index % texture_size) * 3;
            const double u = (block_col + 1.5) / actual_texture_size;
            const double v = (block_row + 1.5) / actual_texture_size;

            // Append UVs
            for (const uint32_t _ : cluster.vertex_indices) {
                mesh.uvs.emplace_back(u, v);
            }

            // Fill cluster color in texture
            const Rgb &color = cluster_colors[cluster_index];
            texture.at<cv::Vec3b>(block_row + 1, block_col + 1) = {color.z, color.y, color.x};
        }

        mesh.texture = texture;
    } else {
        if (clustering.textures.size() == 1) {
            // Just copy the single texture
            mesh.texture = clustering.textures[0];
        } else if (!clustering.textures.empty()) {
            // There are multiple textures so we need to create an atlas
            const std::vector<glm::uvec2> texture_sizes = transform_vector(clustering.textures, [](const auto &texture) {
                return glm::uvec2(texture.cols, texture.rows);
            });
            const atlas::Plan plan = atlas::plan(texture_sizes);

            // remap the uvs to match the atlas
            uint32_t uv_offset = 0;
            for (const Cluster &cluster : clustering.clusters) {
                if (any_has_uvs && cluster.is_textured()) {
                    std::span<glm::dvec2> cluster_uvs(mesh.uvs.data() + uv_offset, cluster.vertex_count());
                    atlas::map_uvs(plan, cluster.texture_id.value(), cluster_uvs);
                }
                uv_offset += cluster.vertex_count();
            }

            // Create the atlas texture
            mesh.texture = atlas::create(plan, clustering.textures);
        }
    }

    return mesh;
}
}

inline mesh::Simple clustering_to_mesh(const Clustering &clustering, const bool debug_texture = false) {
    const Clustering manifold_clustering = make_manifold(clustering);
    return detail::manifold_clustering_to_mesh(manifold_clustering, debug_texture);
}

// like clustering_to_mesh but ignores textures.
inline mesh::Simple clustering_to_textureless_mesh(const Clustering &clustering) {
    mesh::Simple mesh;
    mesh.positions = clustering.positions;
    for (const Cluster &cluster : clustering.clusters) {
        for (const glm::uvec3 &local_triangle : cluster.local_triangles) {
            mesh.triangles.emplace_back(
                cluster.vertex_indices[local_triangle.x],
                cluster.vertex_indices[local_triangle.y],
                cluster.vertex_indices[local_triangle.z]);
        }
    }
    return mesh;
}

inline void trim_textures_inplace(Clustering &clustering) {
    // Group clusters by texture
    std::vector<std::vector<uint32_t>> clusters_per_texture(clustering.textures.size());
    for (const auto& [i, cluster] : enumerate(clustering.clusters)) {
        if (cluster.is_textured()) {
            clusters_per_texture[cluster.texture_id.value()].push_back(i);
        }
    }

    // Trim each texture one by one
    for (const auto& [texture_id, clusters] : enumerate(clusters_per_texture)) {
        // Compute UV bounds for this texture
        radix::geometry::Aabb2d uv_bounds;
        for (const uint32_t cluster_index : clusters) {
            const Cluster &cluster = clustering.clusters[cluster_index];
            const std::span<const glm::dvec2> cluster_uvs = cluster.uvs;
            const radix::geometry::Aabb2d cluster_uv_bounds = radix::geometry::find_bounds(cluster_uvs);
            uv_bounds.expand_by(cluster_uv_bounds);
        }

        // Trim the texture and calculate the UV remap
        cv::Mat &texture = clustering.textures[texture_id];
        TextureTrim trim = compute_texture_trim(texture, uv_bounds);
        texture = trim.texture;

        // Apply the trim to all clusters using this texture
        for (const uint32_t cluster_index : clusters) {
            Cluster &cluster = clustering.clusters[cluster_index];
            std::span<glm::dvec2> cluster_uvs = cluster.uvs;
            trim.uv_remap.remap_uvs_inplace(cluster_uvs);
        }
    }
}
inline Clustering trim_textures(const Clustering &clustering) {
    Clustering trimmed = clustering;
    trim_textures_inplace(trimmed);
    return trimmed;
}
