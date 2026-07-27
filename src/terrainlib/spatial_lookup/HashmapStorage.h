#pragma once

#include <unordered_map>

#include <glm/glm.hpp>

#include "quantize.h"
#include "spatial_lookup/CellBasedStorage.h"
#include "spatial_lookup/QuantizedHash.h"

namespace spatial_lookup {

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

    [[nodiscard]] size_t point_count() const {
        return this->_store.size();
    }

    [[nodiscard]] CellIndex point_to_cell_index(const Vec &point) const {
        return CellIndex(quantize_floor(point, this->_epsilon));
    }
    [[nodiscard]] CellIndex offset_cell_index(const CellIndex &index, const glm::vec<n_dims, int32_t> &offset) const {
        return point_to_cell_index(index.quantized + (Vec(offset) + Component(0.5)) * this->_epsilon);
    }

    [[nodiscard]] Bounds cell_bounds(const CellIndex &index) const {
        return Bounds {
            .min = index.quantized,
            .max = index.quantized + Vec(this->_epsilon)
        };
    }

    bool insert(const Vec &point, Value value) {
        this->_store.emplace(point, std::move(value));
        return true;
    }

    template <typename Func>
    bool for_all_in_cell(const CellIndex &index, Func &&func) const {
        return for_all_in_cell_impl(*this, index, std::forward<Func>(func));
    }

    template <typename Func>
    bool for_all_in_cell(const CellIndex &index, Func &&func) {
        return for_all_in_cell_impl(*this, index, std::forward<Func>(func));
    }

    template <typename Func>
    bool for_all_points(Func &&func) const {
        return for_all_points_impl(*this, std::forward<Func>(func));
    }

    template <typename Func>
    bool for_all_points(Func &&func) {
        return for_all_points_impl(*this, std::forward<Func>(func));
    }

private:
    template <typename SelfT, typename Func>
    static bool for_all_in_cell_impl(SelfT &self, const CellIndex &index, Func &&func) {
        auto [begin, end] = self._store.equal_range(index.quantized);

        bool any_found = false;
        for (auto it = begin; it != end; ++it) {
            any_found = true;
            func(it->first, it->second);
        }
        return any_found;
    }

    template <typename SelfT, typename Func>
    static bool for_all_points_impl(SelfT &self, Func &&func) {
        bool any_found = false;
        for (auto &[point, value] : self._store) {
            any_found = true;
            func(point, value);
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
