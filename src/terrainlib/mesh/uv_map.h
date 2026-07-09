#pragma once

#include <filesystem>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/cgal.h"

typedef cv::Mat Texture;

namespace uv_map {

enum class Algorithm {
    TutteBarycentricMapping,
    DiscreteAuthalic,
    DiscreteConformalMap,
    FloaterMeanValueCoordinates
};

enum class Border {
    Circle,
    Square
};

class UvParameterizationError {
public:
    UvParameterizationError() = default;
    constexpr UvParameterizationError(int code)
        : code(code) {}

    operator int() const {
        return this->code;
    }

    std::string description() const;

private:
    int code;
};

tl::expected<void, UvParameterizationError> parameterize_mesh(
    cgal::Mesh &mesh,
    Algorithm algorithm,
    Border border);

tl::expected<std::vector<glm::dvec2>, UvParameterizationError> parameterize_mesh(
    const SimpleMesh &mesh,
    Algorithm algorithm,
    Border border);

void warp_triangle(
    const cv::Mat &source_image,
    cv::Mat &target_image,
    std::array<cv::Point2f, 3> source_triangle,
    std::array<cv::Point2f, 3> target_triangle);

Texture merge_textures(
    const std::span<const std::reference_wrapper<const SimpleMesh>> original_meshes,
    const SimpleMesh &merged_mesh,
    const mesh::merging::VertexMapping &mapping,
    const glm::uvec2 merged_texture_size);

/*
Texture merge_textures(
    const std::span<const std::reference_wrapper<const SimpleMesh>> original_meshes,
    const SimpleMesh& merged_mesh,
    const mesh::merging::VertexMapping &mapping,
    const glm::uvec2 merged_texture_size);
    */

}
