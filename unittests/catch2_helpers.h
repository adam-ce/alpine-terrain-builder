/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_tostring.hpp>
#include <glm/glm.hpp>
#include <radix/geometry.h>
#include <fmt/format.h>

#include "fmt_impls.h"
#include "type_utils.h"

namespace Catch {
template <glm::length_t N, typename T>
struct StringMaker<glm::vec<N, T>> {
    static std::string convert(const glm::vec<N, T> &value) {
        return fmt::format("{}", value);
    }
};

template <glm::length_t N, typename T>
struct StringMaker<radix::geometry::Aabb<N, T>> {
    static std::string convert(const radix::geometry::Aabb<N, T> &value) {
        return fmt::format("Aabb{}{}(({}, {}, {}) - ({}, {}, {}))", 
            N,
            type_name<T>()[0], 
            value.min.x, value.min.y, value.min.z,
            value.max.x, value.max.y, value.max.z);
    }
};
}
