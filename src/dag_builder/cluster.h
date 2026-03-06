#pragma once

#include <vector>
#include <optional>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "octree/Id.h"

struct Cluster {
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
    // std::vector<cv::Mat> textures;
    std::optional<cv::Mat> texture;

    constexpr size_t vertex_count() const noexcept {
        return this->positions.size();
    }
    constexpr size_t cluster_count() const noexcept {
        return this->clusters.size();
    }
};
