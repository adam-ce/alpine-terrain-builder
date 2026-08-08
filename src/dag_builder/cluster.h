#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "TextureSet.h"
#include "range_utils.h"

inline constexpr uint32_t MAX_TRIANGLES_PER_CLUSTER = 256;

struct Cluster {
    uint32_t id = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> vertex_indices; // indices into Clustering::positions
    std::vector<glm::uvec3> local_triangles; // indices into this->vertex_indices
    std::vector<glm::dvec2> uvs; // per local vertex 

    std::optional<uint32_t> texture_id; // index into Clustering::textures
    double absolute_error = 0.0; // absolute error of this cluster compared to original mesh

    constexpr uint32_t vertex_count() const noexcept {
        return this->vertex_indices.size();
    }
    constexpr uint32_t triangle_count() const noexcept {
        return this->local_triangles.size();
    }
    constexpr bool has_uvs() const noexcept {
        return !this->uvs.empty();
    }
    constexpr bool has_texture() const noexcept {
        return this->texture_id.has_value();
    }
    glm::uvec3 global_triangle(const glm::uvec3 &local_triangle) const {
        return {
            this->vertex_indices[local_triangle.x],
            this->vertex_indices[local_triangle.y],
            this->vertex_indices[local_triangle.z]};
    }
};

struct Clustering {
    std::vector<glm::dvec3> positions;
    std::vector<Cluster> clusters;
    TextureSet textures = {};

    constexpr size_t vertex_count() const noexcept {
        return this->positions.size();
    }
    constexpr size_t cluster_count() const noexcept {
        return this->clusters.size();
    }
    constexpr bool is_empty() const noexcept {
        return this->vertex_count() == 0 || this->cluster_count() == 0;
    }
    std::vector<glm::dvec3> get_cluster_positions(const uint32_t cluster_index) const {
        return transform_vector(this->clusters[cluster_index].vertex_indices, [&](const uint32_t global) {
            return this->positions[global];
        });
    }
    std::optional<cv::Mat> get_cluster_texture(const uint32_t cluster_index) const noexcept {
        const Cluster &cluster = this->clusters[cluster_index];
        if (!cluster.has_texture()) {
            return std::nullopt;
        }
        return this->textures[cluster.texture_id.value()];
    }
};
