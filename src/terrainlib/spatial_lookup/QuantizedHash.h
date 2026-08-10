#pragma once

#include <glm/glm.hpp>

#include "VecHash.h"
#include "numeric/quantize.h"

namespace spatial_lookup {
namespace detail {

template <glm::length_t n_dims, typename T>
struct QuantizedVecHash {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecHash(T epsilon) : epsilon(epsilon) {}

    T epsilon;

    size_t operator()(const Vec &v) const noexcept {
        const Vec quantized = quantize_floor(v, this->epsilon);
        return VecHash<n_dims, T>{}(quantized);
    }
};

template <glm::length_t n_dims, typename T>
struct QuantizedVecEqual {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecEqual(T epsilon) : epsilon(epsilon) {}

    T epsilon;
    bool operator()(const Vec &a, const Vec &b) const noexcept {
        return quantize_floor(a, this->epsilon) == quantize_floor(b, this->epsilon);
    }
};

} // namespace detail
} // namespace spatial_lookup
