#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include <limits>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "TextureSet.h"

struct Cluster {
    uint32_t id = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> vertex_indices; // indices into Clustering::positions
    std::vector<glm::uvec3> local_triangles; // indices into this->vertex_indices
    std::vector<glm::dvec2> uvs; // per local vertex 

    uint32_t texture_id = 0; // index into Clustering::textures
    double absolute_error = 0.0; // absolute error of this cluster compared to original mesh

    constexpr size_t vertex_count() const noexcept {
        return this->vertex_indices.size();
    }
    constexpr size_t triangle_count() const noexcept {
        return this->local_triangles.size();
    }
    constexpr bool has_uvs() const noexcept {
        return !this->uvs.empty();
    }
};

struct Clustering {
    std::vector<glm::dvec3> positions;
    std::vector<Cluster> clusters;
    TextureSet textures;

    constexpr size_t vertex_count() const noexcept {
        return this->positions.size();
    }
    constexpr size_t cluster_count() const noexcept {
        return this->clusters.size();
    }
    constexpr bool is_empty() const noexcept {
        return this->vertex_count() == 0 || this->cluster_count() == 0;
    }
    cv::Mat get_cluster_texture(const uint32_t cluster_index) const noexcept {
        return this->textures[this->clusters[cluster_index].texture_id];
    }
};
