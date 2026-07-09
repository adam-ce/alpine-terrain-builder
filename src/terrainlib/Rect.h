#pragma once

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

using Rect2d = Rect2<double>;
using Rect2f = Rect2<float>;
using Rect2i = Rect2<int>;
using Rect2ui = Rect2<unsigned int>;

using Rect3d = Rect3<double>;
using Rect3f = Rect3<float>;
using Rect3i = Rect3<int>;
using Rect3ui = Rect3<unsigned int>;
