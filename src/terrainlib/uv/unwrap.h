/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
