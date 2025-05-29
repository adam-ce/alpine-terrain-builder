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

#include <optional>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <glm/gtx/string_cast.hpp>

#include "octree/NodeStatus.h"

namespace Catch {
template <glm::length_t s, typename T>
struct StringMaker<glm::vec<s, T>> {
    static std::string convert(const glm::vec<s, T>& value) {
        return glm::to_string(value);
    }
};

template<>
struct StringMaker<octree::NodeStatus> {
    static std::string convert(const octree::NodeStatus& status) {
        return status.to_string();
    }
};

template <typename T>
struct StringMaker<std::optional<T>> {
    static std::string convert(const std::optional<T>& opt) {
        if (opt.has_value()) {
            return StringMaker<std::remove_cv_t<std::remove_reference_t<T>>>::convert(opt.value());
        } else {
            return "nullopt";
        }
    }
};

}
