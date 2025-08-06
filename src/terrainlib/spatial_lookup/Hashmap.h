#pragma once

#include <unordered_map>
#include <vector>

#include <radix/geometry.h>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtx/norm.hpp>
#include <libassert/assert.hpp>

#include "NDLoopHelper.h"
#include "SpatialLookup.h"
#include "hash_utils.h"

namespace spatial_lookup {
namespace {
template <typename T>
T remainder(const T x, const T y) {
    if constexpr (std::is_integral<T>::value) {
        return x % y;
    } else {
        return std::fmod(x, y);
    }
}

template <typename T>
T quantize(const T x, const T epsilon) {
    return x - remainder(x, epsilon);
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> remainder(const glm::vec<n_dims, T> &v, const T epsilon) {
    glm::vec<n_dims, T> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = remainder(v[i], epsilon);
    }
    return result;
}

template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> quantize(const glm::vec<n_dims, T>& v, const T epsilon) {
    glm::vec<n_dims, T> result;
    for (glm::length_t i = 0; i < n_dims; i++) {
        result[i] = quantize(v[i], epsilon);
    }
    return result;
}

template <glm::length_t n_dims, typename T>
struct VecHash {
    using Vec = glm::vec<n_dims, T>;

    T epsilon;

    size_t operator()(const Vec &v) const noexcept {
        size_t seed = hash::default_seed();
        for (glm::length_t i = 0; i < n_dims; i++) {
            hash::append(seed, quantize(v[i], epsilon));
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
class Hashmap : public SpatialLookup<n_dims, Component, Value> {
public:
    using Self = Hashmap<n_dims, Component, Value>;
    using Base = SpatialLookup<n_dims, Component, Value>;
    using Vec = glm::vec<n_dims, Component>;
    using Equal = NeverEqual<Vec>;
    using Hash = VecHash<n_dims, Component>;
    using Container = std::unordered_map<Vec, Value, Hash, Equal>;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using Bounds = radix::geometry::Aabb<n_dims, Component>;

    explicit Hashmap(Component epsilon)
        : _epsilon(epsilon),
          _store(0, Hash(epsilon), Equal()) {}

    void clear() override {
        this->_store.clear();
        this->_bounds = Bounds();
    }

    [[nodiscard]] bool empty() const {
        return this->_store.empty();
    }

    [[nodiscard]] size_t size() const {
        return this->_store.size();
    }
    
    void insert(const Vec& point, const Value value) override {
        this->_store.emplace(point, std::move(value));
        this->_bounds.expand_by(point);
    }

    template <typename Distance, typename Func>
    void for_all_near(const Vec &point, const Distance epsilon, Func &&func) {
        const_cast<const Self *>(this)->for_all_near<Distance>(point, epsilon, [&](const Vec &vec, const Value &value, const Distance distance_sq) {
            func(vec, const_cast<Value &>(value), distance_sq);
        });
    }
    template<typename _Distance, typename Func>
    void for_all_near(const Vec &key, const _Distance _lookup_epsilon, Func &&func) const {
        using Distance = MoreExact<_Distance, Component>;
        const Distance lookup_epsilon = _lookup_epsilon;
        DEBUG_ASSERT(lookup_epsilon > 0);

        const Distance lookup_epsilon2 = lookup_epsilon * lookup_epsilon;
        if (radix::geometry::distance_sq(this->_bounds, key) > lookup_epsilon2) {
            return;
        }

        const uint32_t lookup_radius = this->_find_lookup_radius(key, lookup_epsilon);
        const Vec quantized_key = this->_quantize(key);
        const Vec base_key = quantized_key + Vec(this->_epsilon * 0.5f);

        glm::vec<n_dims, int32_t> offset;
        NDLoopHelper<n_dims>::for_each_offset(lookup_radius, offset, [&](const glm::vec<n_dims, int32_t> &offset) {
            const Vec neighbor_key = base_key + Vec(offset) * this->_epsilon;
            size_t bucket_idx = this->_store.bucket(neighbor_key);
            for (auto it = _store.begin(bucket_idx); it != _store.end(bucket_idx); ++it) {
                const Vec &point = it->first;
                const Value &value = it->second;

                if (this->_quantize(neighbor_key) != this->_quantize(point)) {
                    continue; // Skip if the key does not match
                }

                const Distance distance2 = helpers::distance_sq(key, point);
                if (distance2 < lookup_epsilon2) {
                    func(point, value, distance2);
                }
            }
        });
    }
    void for_all_near(const Vec &point, const Component epsilon, typename Base::ForAllNear func) override {
        this->for_all_near<Component, typename Base::ForAllNear>(point, epsilon, std::move(func));
    }
    void for_all_near(const Vec &point, const Component epsilon, typename Base::ForAllNearConst func) const override {
        this->for_all_near<Component, typename Base::ForAllNearConst>(point, epsilon, std::move(func));
    }

    template <typename Distance>
    std::optional<std::reference_wrapper<Value>> find_nearest(const Vec &point, Distance epsilon) {
        return helpers::find_nearest<Self, Vec, Distance, Value>(*this, point, epsilon);
    }
    template <typename Distance>
    std::optional<std::reference_wrapper<const Value>> find_nearest(const Vec &point, Distance epsilon) const {
        return helpers::find_nearest<const Self, Vec, Distance, const Value>(*this, point, epsilon);
    }
    std::optional<std::reference_wrapper<Value>> find_nearest(const Vec &point, const Component epsilon) override {
        return this->find_nearest<Component>(point, epsilon);
    }
    std::optional<std::reference_wrapper<const Value>> find_nearest(const Vec &point, const Component epsilon) const override {
        return this->find_nearest<Component>(point, epsilon);
    }

    template <typename Distance, typename Vector>
    bool find_all_near(const Vec &point, Distance epsilon, Vector &out) {
        return helpers::find_all_near<Self, Vec, Distance, Vector, Value>(*this, point, epsilon, out);
    }
    template <typename Distance, typename Vector>
    bool find_all_near(const Vec &point, Distance epsilon, Vector &out) const {
        return helpers::find_all_near<const Self, Vec, Distance, Vector, const Value>(*this, point, epsilon, out);
    }
    bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<Value>> &out) override {
        return this->find_all_near<Component, std::vector<std::reference_wrapper<Value>>>(point, epsilon, out);
    }
    bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<const Value>> &out) const override {
        return this->find_all_near<Component, std::vector<std::reference_wrapper<const Value>>>(point, epsilon, out);
    }

private:
    Vec _quantize(const Vec &v) const {
        return quantize(v, this->_epsilon);
    }

    template <typename Distance>
    uint32_t _find_lookup_radius(const Vec &key, const Distance lookup_epsilon) const {
        const Distance store_epsilon = this->_epsilon;
        if (lookup_epsilon < store_epsilon) {
            // Check if it is sufficient to check only the cell of the key
            const auto quantization_remainder = remainder(glm::vec<n_dims, Distance>(key), Distance(this->_epsilon));
            const Distance distance_to_next_key = std::min(
                glm::compMin(quantization_remainder),
                Distance(this->_epsilon) - glm::compMax(quantization_remainder));
            DEBUG_ASSERT(distance_to_next_key >= 0);
            DEBUG_ASSERT(distance_to_next_key * 2 < this->_epsilon);
            if (distance_to_next_key > lookup_epsilon) {
                return 0;
            }
        }

        const uint32_t lookup_radius = static_cast<uint32_t>(std::ceil(lookup_epsilon / this->_epsilon));
        if (lookup_radius > 1) {
            LOG_WARN("Lookup epsilon {} is large compared to quantization epsilon {} resulting in lookup radius of {}",
                        lookup_epsilon, this->_epsilon, lookup_radius);
        }
        return lookup_radius;
    }

    /*const*/ Component _epsilon;
    Container _store;
    Bounds _bounds;

    static_assert(SpatialLookupExt<Self, Vec, Component, Value>);
};
}
