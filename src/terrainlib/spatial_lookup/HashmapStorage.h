#pragma once

#include <unordered_map>

#include <glm/glm.hpp>

#include "spatial_lookup/CellBasedStorage.h"
#include "hash_utils.h"

namespace spatial_lookup {
namespace {
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
struct VecHash {
    using Vec = glm::vec<n_dims, T>;

    explicit VecHash(T epsilon) : epsilon(epsilon) {}

    T epsilon;

    size_t operator()(const Vec &v) const noexcept {
        size_t seed = hash::default_seed();
        for (glm::length_t i = 0; i < n_dims; i++) {
            hash::append(seed, quantize(v[i], this->epsilon));
        }
        return seed;
    }
};

template <typename T>
struct NeverEqual {
    bool operator()(const T &, const T &) const noexcept {
        return false;
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
        : _epsilon(epsilon), _store(0, Hash(epsilon), Equal()) {
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
        return CellIndex(quantize(point, this->_epsilon));
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
        const size_t bucket_idx = this->_store.bucket(index.quantized + Vec(0.5 * this->_epsilon));
        bool any_found = false;

        for (auto it = this->_store.begin(bucket_idx); it != this->_store.end(bucket_idx); it++) {
            const Vec &point = it->first;
            const Value &value = it->second;

            if (index.quantized != quantize(point, this->_epsilon)) {
                continue; // Skip if the key does not match
            }

            any_found = true;
            func(point, value);
        }

        return any_found;
    }
    template <typename Func>
    bool for_all_in_cell(const CellIndex index, Func &&func) {
        return const_cast<const Self *>(this)->for_all_in_cell(index, [&](const Vec &vec, const Value &value) {
            func(vec, const_cast<Value &>(value));
        });
    }

private:
    using Hash = VecHash<n_dims, Component>;
    using Equal = NeverEqual<Vec>;

    Component _epsilon; 
    std::unordered_map<Vec, Value, Hash, Equal> _store;

    // static_assert(CellBasedStorage<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
