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
#include <unordered_map>
#include <utility>

#include <glm/glm.hpp>

#include "VecHash.h"
#include "numeric/quantize.h"
#include "spatial_lookup/CellBasedStorage.h"

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
class HashmapStorage {
public:
    using Self = HashmapStorage<n_dims, Component, Value>;
    using Vec = glm::vec<n_dims, Component>;
    using Bounds = radix::geometry::Aabb<n_dims, Component>;
    using CellIndex = glm::vec<n_dims, int64_t>;

    explicit HashmapStorage(Component epsilon)
        : _epsilon(epsilon) {
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
        return quantize_index(point, this->_epsilon);
    }
    [[nodiscard]] CellIndex offset_cell_index(const CellIndex &index, const glm::vec<n_dims, int32_t> &offset) const {
        return index + CellIndex(offset);
    }

    [[nodiscard]] Bounds cell_bounds(const CellIndex &index) const {
        const Vec min = Vec(index) * this->_epsilon;
        return Bounds {
            .min = min,
            .max = min + Vec(this->_epsilon)
        };
    }

    bool insert(const Vec &point, Value value) {
        this->_store.emplace(this->point_to_cell_index(point), Entry{point, std::move(value)});
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
    struct Entry {
        Vec point;
        Value value;
    };

    template <typename SelfT, typename Func>
    static bool for_all_in_cell_impl(SelfT &self, const CellIndex &index, Func &&func) {
        auto [begin, end] = self._store.equal_range(index);

        bool any_found = false;
        for (auto it = begin; it != end; ++it) {
            any_found = true;
            func(it->second.point, it->second.value);
        }
        return any_found;
    }

    template <typename SelfT, typename Func>
    static bool for_all_points_impl(SelfT &self, Func &&func) {
        bool any_found = false;
        for (auto &[index, entry] : self._store) {
            any_found = true;
            func(entry.point, entry.value);
        }
        return any_found;
    }

    Component _epsilon;
    std::unordered_multimap<CellIndex, Entry, VecHash<n_dims, int64_t>> _store;

    // static_assert(CellBasedStorage<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
