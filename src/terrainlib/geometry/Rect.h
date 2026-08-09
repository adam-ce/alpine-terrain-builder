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
