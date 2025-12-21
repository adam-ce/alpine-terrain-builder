#pragma once

#include <vector>
#include <optional>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "octree/Id.h"

struct DagId {
    octree::Id node_id;
    uint8_t level_in_node;
    uint32_t index_on_level;
};

struct UvUnwrapping {
    std::optional<cv::Mat> texture;
    std::vector<glm::dvec3> uvs;
};

struct Cluster {
    // DagId id;
    std::vector<uint32_t> vertex_indices;
    std::vector<glm::uvec3> local_triangles;
    std::optional<UvUnwrapping> uv_unwrapping;
    double relative_error = 0.0;

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
};
