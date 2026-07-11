#pragma once

#include <span>
#include <vector>
#include <tl/expected.hpp>
#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace uv {

enum class Algorithm {
    TutteBarycentricMapping,
    DiscreteAuthalic,
    DiscreteConformalMap,
    FloaterMeanValueCoordinates,
    LeastSquaresConformalMap,
    AsRigidAsPossible
};

inline constexpr bool is_free_border(Algorithm algorithm) {
    return algorithm == Algorithm::LeastSquaresConformalMap ||
           algorithm == Algorithm::AsRigidAsPossible;
}

enum class Border {
    Circle,
    Square
};

class UnwrapError {
public:
    UnwrapError() = default;
    constexpr UnwrapError(int code) : code(code) {}

    operator int() const {
        return this->code;
    }

    std::string description() const;

private:
    int code;
};

using Map = std::vector<glm::dvec2>;
using Texture = cv::Mat;

inline constexpr Algorithm DEFAULT_ALGORITHM = Algorithm::TutteBarycentricMapping;
inline constexpr Border DEFAULT_BORDER = Border::Circle;

tl::expected<Map, UnwrapError> unwrap(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER);

inline tl::expected<Map, UnwrapError> unwrap(
    const std::vector<glm::uvec3>& triangles,
    const std::vector<glm::dvec3>& positions,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER) {
    return unwrap(
        std::span{triangles},
        std::span{positions},
        algorithm,
        border);
}

inline tl::expected<Map, UnwrapError> unwrap(
    const mesh::View &mesh,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER) {
    return unwrap(
        mesh.triangles,
        mesh.positions,
        algorithm,
        border);
}

inline tl::expected<Map, UnwrapError> unwrap(
    const mesh::Simple &mesh,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER) {
    return unwrap(
        mesh.triangles,
        mesh.positions,
        algorithm,
        border);
}

}
