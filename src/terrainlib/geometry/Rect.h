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

#include <cstdint>

#include <glm/glm.hpp>

template <glm::length_t D, typename T>
struct Rect {
    glm::vec<D, T> position = {};
    glm::vec<D, T> size = {};
};

template <typename T>
using Rect2 = Rect<2, T>;
template <typename T>
using Rect3 = Rect<3, T>;

using Rect2f = Rect2<float>;
using Rect2d = Rect2<double>;
using Rect2i = Rect2<int32_t>;
using Rect2ui = Rect2<uint32_t>;
using Rect2l = Rect2<int64_t>;
using Rect2ul = Rect2<uint64_t>;

using Rect3f = Rect3<float>;
using Rect3d = Rect3<double>;
using Rect3i = Rect3<int32_t>;
using Rect3ui = Rect3<uint32_t>;
using Rect3l = Rect3<int64_t>;
using Rect3ul = Rect3<uint64_t>;
