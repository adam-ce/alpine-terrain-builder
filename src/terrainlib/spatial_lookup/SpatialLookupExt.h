#pragma once

#include <optional>

#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>

namespace spatial_lookup {

template <typename T, typename Vec, typename Distance, typename Value>
concept SpatialLookupExt = requires(
    T t,
    const T ct,
    const Vec &point,
    Value &value_ref,
    Value value,
    Distance epsilon) {
    { t.clear() } -> std::same_as<void>;
    { t.insert(point, value) } -> std::same_as<void>;

    { t.find_nearest(point, epsilon) } -> std::same_as<std::optional<std::reference_wrapper<Value>>>;
    { ct.find_nearest(point, epsilon) } -> std::same_as<std::optional<std::reference_wrapper<const Value>>>;

    requires requires(std::vector<Value> &out) {
        { out.emplace_back(value) };

        { t.find_all_near(point, epsilon, out) } -> std::same_as<bool>;
        { ct.find_all_near(point, epsilon, out) } -> std::same_as<bool>;
    };

    {
        t.for_all_near(point, epsilon, [](const Vec &, Value &, decltype(epsilon)){})
    } -> std::same_as<void>;
    {
        ct.for_all_near(point, epsilon, [](const Vec &, const Value &, decltype(epsilon)) {})
    } -> std::same_as<void>;
};

namespace helpers {

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

template <
    typename SpatialLookup,
    typename Vec,
    typename Distance,
    typename Value>
std::optional<std::reference_wrapper<Value>> find_nearest(
    SpatialLookup &lookup,
    const Vec &point,
    const Distance epsilon) {
    assert(epsilon > 0);
    Distance closest_distance2 = std::numeric_limits<Distance>::max();
    Value* closest_value = nullptr;

    lookup.for_all_near(point, epsilon, [&](const Vec&, Value& value, Distance dist2) {
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

template <
    typename SpatialLookup,
    typename Vec,
    typename Distance,
    typename Vector,
    typename Value>
bool find_all_near(
    SpatialLookup &lookup,
    const Vec &point,
    const Distance epsilon,
    Vector &out) {
    out.clear();
    lookup.for_all_near(point, epsilon, [&](const Vec&, Value& value, Distance) {
        out.emplace_back(value);
    });
    return !out.empty();
}
}

}
