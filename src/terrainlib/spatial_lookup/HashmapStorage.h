#pragma once

#include <cmath>
#include <unordered_map>

#include <glm/glm.hpp>

#include "spatial_lookup/CellBasedStorage.h"
#include "VecHash.h"

namespace spatial_lookup {
namespace detail {
template <typename T>
T quantize(const T x, const T epsilon) {
    //  x - remainder(x, epsilon) is not idempotent
    return std::floor(x / epsilon) * epsilon;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize(const glm::vec<n_dims, T> &v, const T epsilon) {
    glm::vec<n_dims, T> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = quantize(v[i], epsilon);
    }
    return result;
}

template <glm::length_t n_dims, typename T>
struct QuantizedVecHash {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecHash(T epsilon) : epsilon(epsilon) {}

    T epsilon;

    size_t operator()(const Vec &v) const noexcept {
        const Vec quantized = quantize(v, this->epsilon);
        return VecHash<n_dims, T>{}(quantized);
    }
};

template <glm::length_t n_dims, typename T>
struct QuantizedVecEqual {
    using Vec = glm::vec<n_dims, T>;

    explicit QuantizedVecEqual(T epsilon) : epsilon(epsilon) {}

    T epsilon;
    bool operator()(const Vec& a, const Vec& b) const noexcept {
        return quantize(a, this->epsilon) == quantize(b, this->epsilon);
    }
};

} // namespace

template <glm::length_t n_dims, typename Component, typename Value>
class HashmapStorage {
public:
    using Self = HashmapStorage<n_dims, Component, Value>;
    using Vec = glm::vec<n_dims, Component>;
    using Bounds = radix::geometry::Aabb<n_dims, Component>;

    struct CellIndex {
        explicit CellIndex(const Vec& v) : quantized(v) {}

        const Vec quantized;
    };

    explicit HashmapStorage(Component epsilon)
        : _epsilon(epsilon), _store(0, Hash(epsilon), Equal(epsilon)) {
    }

    void clear() {
        this->_store.clear();
    }

    [[nodiscard]] bool empty() const {
        return this->_store.empty();
    }

    [[nodiscard]] size_t size() const {
        return this->_store.size();
    }

    [[nodiscard]] CellIndex point_to_cell_index(const Vec &point) const {
        return CellIndex(detail::quantize(point, this->_epsilon));
    }
    [[nodiscard]] CellIndex offset_cell_index(const CellIndex index, const glm::vec<n_dims, int32_t> &offset) const {
        return point_to_cell_index(index.quantized + (Vec(offset) + Component(0.5)) * this->_epsilon);
    }

    [[nodiscard]] Bounds cell_bounds(const CellIndex index) const {
        return Bounds {
            .min = index.quantized,
            .max = index.quantized + this->_epsilon
        };
    }

    bool insert(const Vec &point, const Value value) {
        this->_store.emplace(point, std::move(value));
        return true;
    }

    template <typename Func>
    bool for_all_in_cell(const CellIndex index, Func &&func) const {
        return for_all_in_cell_impl(*this, index, std::forward<Func>(func));
    }

    template <typename Func>
    bool for_all_in_cell(const CellIndex index, Func &&func) {
        return for_all_in_cell_impl(*this, index, std::forward<Func>(func));
    }

private:
    template <typename SelfT, typename Func>
    static bool for_all_in_cell_impl(SelfT &self, const CellIndex index, Func &&func) {
        auto [begin, end] = self._store.equal_range(index.quantized);

        bool any_found = false;

        for (auto it = begin; it != end; ++it) {
            any_found = true;
            func(it->first, it->second);
        }

        return any_found;
    }

    using Hash = detail::QuantizedVecHash<n_dims, Component>;
    using Equal = detail::QuantizedVecEqual<n_dims, Component>;

    Component _epsilon; 
    std::unordered_multimap<Vec, Value, Hash, Equal> _store;

    // static_assert(CellBasedStorage<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
