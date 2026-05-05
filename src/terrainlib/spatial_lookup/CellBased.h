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

namespace detail {
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

    template <typename Distance, typename Func>
    bool for_all_near(const Vec &point, Distance epsilon, Func &&func) const {
        return for_all_near_impl<const Self>(*this, point, epsilon, std::forward<Func>(func));
    }

    template <typename Distance, typename Func>
    bool for_all_near(const Vec &point, Distance epsilon, Func &&func) {
        return for_all_near_impl<Self>(*this, point, epsilon, std::forward<Func>(func));
    }

    template <typename Distance>
    std::optional<std::reference_wrapper<const Value>> find_nearest(
        const Vec &point,
        const Distance epsilon) const {
        return find_nearest_impl<const Self>(*this, point, epsilon);
    }
    template <typename Distance>
    std::optional<std::reference_wrapper<Value>> find_nearest(
        const Vec &point,
        const Distance epsilon) {
        return find_nearest_impl<Self>(*this, point, epsilon);
    }

    template <typename Distance, typename Vector>
    bool find_all_near(
        const Vec &point,
        Distance epsilon,
        Vector &out) const {
        const auto actual_epsilon = static_cast<MoreExact<Distance, Component>>(epsilon);
        return find_all_near_impl<const Self>(*this, point, actual_epsilon, out);
    }
    template <typename Distance, typename Vector>
    bool find_all_near(
        const Vec &point,
        Distance epsilon,
        Vector &out) {
        const auto actual_epsilon = static_cast<MoreExact<Distance, Component>>(epsilon);
        return find_all_near_impl<Self>(*this, point, actual_epsilon, out);
    }

    template <typename Func>
    bool for_all_at(const Vec &point, Func &&func) const {
        return for_all_at_impl<const Self>(*this, point, std::forward<Func>(func));
    }

    template <typename Func>
    bool for_all_at(const Vec &point, Func &&func) {
        return for_all_at_impl<Self>(*this, point, std::forward<Func>(func));
    }

    template <typename Vector>
    bool find_all_at(const Vec &point, Vector &out) const {
        return find_all_at_impl<const Self>(*this, point, out);
    }

    template <typename Vector>
    bool find_all_at(const Vec &point, Vector &out) {
        return find_all_at_impl<Self>(*this, point, out);
    }

private:
    Storage _storage;
    Bounds _bounds;

    template <typename Self, typename Distance, typename Func>
    static bool for_all_near_impl(
        Self &self,
        const Self::Vec &point,
        Distance epsilon,
        Func &&func) {
        using ValueRef = std::conditional_t<
            std::is_const_v<Self>,
            const Value &,
            Value &>;

        DEBUG_ASSERT(epsilon > 0);

        const Distance epsilon2 = epsilon * epsilon;
        if (radix::geometry::distance_sq(self._bounds, point) > epsilon2) {
            return false;
        }

        const CellIndex cell_index = self._storage.point_to_cell_index(point);
        const uint32_t lookup_radius = self.find_lookup_radius(cell_index, point, epsilon);

        bool any_found = false;
        NDLoopHelper<n_dims>::for_each_offset(lookup_radius, [&](const glm::vec<n_dims, int32_t> &offset) {
            const CellIndex neighbor_index = self._storage.offset_cell_index(cell_index, offset);
            any_found |= self._storage.for_all_in_cell(neighbor_index, [=](const Vec &neighbor_point, ValueRef value) {
                const Distance distance2 = detail::distance_sq(point, neighbor_point);
                if (distance2 < epsilon2) {
                    func(point, value, distance2);
                }
            });
        });

        return any_found;
    }

    template <typename Self, typename Distance>
    static std::optional<std::reference_wrapper<typename Self::Value>> find_nearest_impl(
        Self &self,
        const typename Self::Vec &point,
        const Distance epsilon) {
        using Vec = Self::Vec;
        using Value = std::conditional_t<
            std::is_const_v<Self>,
            const typename Self::Value,
            typename Self::Value>;

        Distance closest_distance2 = std::numeric_limits<Distance>::max();
        Value *closest_value = nullptr;

        self.for_all_near(point, epsilon, [&](const Vec &, Value &value, Distance dist2) {
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
    static bool find_all_near_impl(
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

    template <typename Distance>
    uint32_t find_lookup_radius(const CellIndex& index, const Vec &point, const Distance epsilon) const {
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

    template <typename Self, typename Func>
    static bool for_all_at_impl(
        Self &self,
        const Vec &point,
        Func &&func) {
        using ValueRef = std::conditional_t<
            std::is_const_v<Self>,
            const Value &,
            Value &>;

        const CellIndex cell_index = self._storage.point_to_cell_index(point);

        return self._storage.for_all_in_cell(
            cell_index,
            [&](const Vec &stored_point, ValueRef value) {
                if (stored_point == point) {
                    func(stored_point, value);
                }
            });
    }

    template <typename Self, typename Vector>
    static bool find_all_at_impl(
        Self &self,
        const Vec &point,
        Vector &out) {
        using ValueRef = std::conditional_t<
            std::is_const_v<Self>,
            const Value &,
            Value &>;

        out.clear();

        self.for_all_at(point, [&](const Vec &, ValueRef value) {
            out.emplace_back(value);
        });

        return !out.empty();
    }

    // static_assert(SpatialLookup<Self, n_dims, Component, Value>);
};

} // namespace spatial_lookup
