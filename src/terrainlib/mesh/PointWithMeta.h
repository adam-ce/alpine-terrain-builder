#pragma once

#include <glm/common.hpp>

template <typename Meta, glm::length_t n_dims = 3, typename Comp = double>
struct PointWithMeta {
    using Vec = glm::vec<n_dims, Comp>;

    Vec point;
    Meta meta;

    operator const Vec &() const {
        return this->point;
    }
};
