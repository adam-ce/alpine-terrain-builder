#pragma once

#include <glm/glm.hpp>

#include "hash_utils.h"

template <glm::length_t n_dims, typename T>
struct VecHash {
    using Vec = glm::vec<n_dims, T>;

    size_t operator()(const Vec &v) const noexcept {
        size_t seed = hash::default_seed();
        for (glm::length_t i = 0; i < n_dims; i++) {
            hash::append(seed, v[i]);
        }
        return seed;
    }
};

using DVec3Hash = VecHash<3, double>;
using DVec2Hash = VecHash<2, double>;
using UVec3Hash = VecHash<3, uint32_t>;
