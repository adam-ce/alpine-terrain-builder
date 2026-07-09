#pragma once

#include <concepts>

#include <glm/common.hpp>

namespace spatial_lookup {

template <typename T, glm::length_t n_dims, typename Component, typename Value>
concept SpatialLookup = requires(
    T t,
    const T ct,
    const glm::vec<n_dims, Component> &point,
    Value value,
    Component epsilon) {
    { t.clear() } -> std::same_as<void>;
    { t.insert(point, value) } -> std::same_as<bool>;

    {
        t.for_all_near(point, epsilon, [](const glm::vec<n_dims, Component> &, Value &, const Component) {})
    } -> std::same_as<bool>;
    {
        ct.for_all_near(point, epsilon, [](const glm::vec<n_dims, Component> &, const Value &, const Component) {})
    } -> std::same_as<bool>;
};

} // namespace spatial_lookup
