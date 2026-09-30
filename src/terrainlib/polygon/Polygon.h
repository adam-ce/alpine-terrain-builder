/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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
