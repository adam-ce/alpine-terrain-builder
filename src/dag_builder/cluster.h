#pragma once

#include <vector>
#include <optional>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "octree/Id.h"

struct Cluster {
    std::vector<uint32_t> vertex_indices;
    std::vector<glm::uvec3> local_triangles;
    std::vector<glm::dvec2> uvs;

    double absolute_error = 0.0;

    constexpr size_t vertex_count() const noexcept {
        return this->vertex_indices.size();
    }
    constexpr size_t triangle_count() const noexcept {
        return this->local_triangles.size();
    }
};

struct Clustering {
    std::vector<glm::dvec3> positions;
    std::vector<Cluster> clusters;
    std::optional<cv::Mat> texture;

    constexpr size_t vertex_count() const noexcept {
        return this->positions.size();
    }
    constexpr size_t cluster_count() const noexcept {
        return this->clusters.size();
    }
};
