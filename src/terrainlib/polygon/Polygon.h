#pragma once

#include <vector>

#include <glm/glm.hpp>

template <glm::length_t dimensions = 3, typename T = double>
class Polygon_ {
public:
    using Point = glm::vec<dimensions, T>;

    Polygon_() = default;
    Polygon_(std::vector<Point> points) : points(std::move(points)) {}

    std::vector<Point> points;

    size_t size() const {
        return this->points.size();
    }
};

using Polygon3d = Polygon_<3, double>;
using Polygon2d = Polygon_<2, double>;
