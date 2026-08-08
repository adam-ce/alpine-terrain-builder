#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

struct UvRef {
    uint32_t map_index = 0;
    glm::uvec3 uvs; // indices into uv_maps[map_index]
};

// The surface before simplification, in the uv spaces the bake reads back through.
struct BakeSource {
    std::span<const glm::dvec3> positions;
    std::vector<glm::uvec3> triangles; // indices into positions
    std::vector<UvRef> uv_triangles; // per triangle
    std::vector<std::vector<glm::dvec2>> uv_maps; // per contributing cluster
    std::vector<cv::Mat> images; // parallel to uv_maps
};