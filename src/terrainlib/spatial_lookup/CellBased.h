#pragma once

#include <glm/common.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>

#include "MoreExact.h"
#include "log.h"
#include "spatial_lookup/CellBasedStorage.h"
#include "spatial_lookup/NDLoopHelper.h"
#include "spatial_lookup/SpatialLookup.h"
#include <glm/gtx/norm.hpp>

namespace spatial_lookup {

namespace {
// Calculate distance^2, works with all types
template <glm::length_t n_dims, typename T1, typename T2>
auto distance_sq(const glm::vec<n_dims, T1> &a, const glm::vec<n_dims, T2> &b) {
    using T = MoreExact<T1, T2>;
    using Vec = glm::vec<n_dims, T>;

    if constexpr (std::is_floating_point_v<T>) {
        return glm::distance2(Vec(a), Vec(b));
    } else {
        const auto diff = a - b;
        const auto diff_sq = diff * diff;
        return glm::compAdd(diff_sq);
    }
}

template <typename SpatialLookup, typename Distance>
std::optional<std::reference_wrapper<typename SpatialLookup::Value>> _find_nearest(
    SpatialLookup &lookup,
    const typename SpatialLookup::Vec &point,
    const Distance epsilon) {
    using Vec = SpatialLookup::Vec;
    using Value = std::conditional_t<
        std::is_const_v<SpatialLookup>,
        const typename SpatialLookup::Value,
        typename SpatialLookup::Value>;

    Distance closest_distance2 = std::numeric_limits<Distance>::max();
    Value *closest_value = nullptr;

    lookup.for_all_near(point, epsilon, [&](const Vec &, Value &value, Distance dist2) {
        if (dist2 < closest_distance2) {
            closest_distance2 = dist2;
            closest_value = &value;
        }
    });

    if (closest_value) {
        return std::ref(*closest_value);
    }
    return std::nullopt;
}

template <typename SpatialLookup, typename Distance, typename Vector>
bool _find_all_near(
    SpatialLookup &lookup,
    const typename SpatialLookup::Vec &point,
    const Distance epsilon,
    Vector &out) {
    using Vec = SpatialLookup::Vec;
    using Value = std::conditional_t<
        std::is_const_v<SpatialLookup>,
        const typename SpatialLookup::Value,
        typename SpatialLookup::Value>;

    out.clear();
    lookup.for_all_near(point, epsilon, [&](const Vec &, Value &value, Distance) {
        out.emplace_back(value);
    });
    return !out.empty();
}
}

template <glm::length_t n_dims, typename Component, typename _Value, CellBasedStorage<n_dims, Component, _Value> Storage>
class CellBased {
public:
    using Self = CellBased<n_dims, Component, _Value, Storage>;
    using Vec = glm::vec<n_dims, Component>;
    using Value = _Value;
    using Bounds = radix::geometry::Aabb<n_dims, Component>;
    using CellIndex = typename Storage::CellIndex;

    template <typename... Args>
    explicit CellBased(Args &&...args) : _storage(std::forward<Args>(args)...) {}

    void clear() {
        this->_storage.clear();
    }

    bool insert(const Vec& point, const Value value) {
        const bool inserted = this->_storage.insert(point, value);
        if (inserted) {
            this->_bounds.expand_by(point);
        }
        return inserted;
    }

    const Bounds &bounds() const {
        return this->_bounds;
    }

    template <typename _Distance, typename Func>
    bool for_all_near(const Vec &point, const _Distance _epsilon, const Func func) const {
        using Distance = MoreExact<_Distance, Component>;
        const Distance epsilon = _epsilon;
        DEBUG_ASSERT(epsilon > 0);

        const Distance epsilon2 = epsilon * epsilon;
        if (radix::geometry::distance_sq(this->_bounds, point) > epsilon2) {
            return false;
        }

        const CellIndex cell_index = this->_storage.point_to_cell_index(point);
        const uint32_t lookup_radius = this->_find_lookup_radius(cell_index, point, epsilon);

        bool any_found = false;
        NDLoopHelper<n_dims>::for_each_offset(lookup_radius, [&](const glm::vec<n_dims, int32_t> &offset) {
            const CellIndex neighbor_index = this->_storage.offset_cell_index(cell_index, offset);
            any_found |= this->_storage.for_all_in_cell(neighbor_index, [=](const Vec &neighbor_point, const Value &value) {
                const Distance distance2 = distance_sq(point, neighbor_point);
                if (distance2 < epsilon2) {
                    func(point, value, distance2);
                }
            });
        });

        return any_found;
    }
    template <typename Distance, typename Func>
    bool for_all_near(const Vec &point, const Distance epsilon, const Func func) {
        return const_cast<const Self *>(this)->for_all_near(point, epsilon, [&](const Vec &vec, const Value &value, const Distance distance_sq) {
            func(vec, const_cast<Value &>(value), distance_sq);
        });
    }

    template <typename Distance>
    std::optional<std::reference_wrapper<const Value>> find_nearest(
        const Vec &point,
        const Distance epsilon) const {
        return _find_nearest(*this, point, epsilon);
    }
    template <typename Distance>
    std::optional<std::reference_wrapper<Value>> find_nearest(
        const Vec &point,
        const Distance epsilon) {
        return _find_nearest(*this, point, epsilon);
    } 

    template <typename Distance, typename Vector>
    bool find_all_near(
        const Vec &point,
        const Distance epsilon,
        Vector &out) const {
        return _find_all_near(*this, point, epsilon, out);
    }
    template <typename Distance, typename Vector>
    bool find_all_near(
        const Vec &point,
        const Distance epsilon,
        Vector &out) {
        return _find_all_near(*this, point, epsilon, out);
    }

private:
    Storage _storage;
    Bounds _bounds;

    template <typename Distance>
    uint32_t _find_lookup_radius(const CellIndex& index, const Vec &point, const Distance epsilon) const {
        const Bounds cell_bounds = this->_storage.cell_bounds(index);
        const Vec cell_size = cell_bounds.size();
        const Vec relative_cell_point = point - cell_bounds.min;
        const Vec distance_from_cell_bounds = glm::min(relative_cell_point, cell_size - relative_cell_point);
        if (glm::all(glm::greaterThanEqual(distance_from_cell_bounds, Vec(epsilon)))) {
            return 0;
        } else {
            const Component max_cell_size = glm::compMax(cell_size);
            const uint32_t radius = std::ceil(epsilon / max_cell_size);
            if (radius > 1) {
                LOG_WARN("Lookup epsilon ({}) is too large compared to cell size {} resulting in cell radius of {}",
                         epsilon, cell_size, radius);
            }
            return radius;
        }
    }

    // static_assert(SpatialLookup<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
