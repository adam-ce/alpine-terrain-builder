#pragma once

#include <span>
#include <vector>
#include <expected>
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

using Uvs = std::vector<glm::dvec2>;
using Texture = cv::Mat;

// A uv map filling the unit square, with the aspect its texture should have.
struct Map {
    Uvs uvs;
    double aspect = 1.0;
};

inline constexpr Algorithm DEFAULT_ALGORITHM = Algorithm::TutteBarycentricMapping;
inline constexpr Border DEFAULT_BORDER = Border::Circle;

std::expected<Map, UnwrapError> unwrap(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER);

inline std::expected<Map, UnwrapError> unwrap(
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

inline std::expected<Map, UnwrapError> unwrap(
    const mesh::View &mesh,
    Algorithm algorithm = DEFAULT_ALGORITHM,
    Border border = DEFAULT_BORDER) {
    return unwrap(
        mesh.triangles,
        mesh.positions,
        algorithm,
        border);
}

inline std::expected<Map, UnwrapError> unwrap(
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
