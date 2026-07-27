#pragma once

#include <concepts>
#include <optional>
#include <vector>

#include <glm/common.hpp>
#include <radix/geometry.h>

namespace spatial_lookup {

template <
    typename T,
    glm::length_t n_dims,
    typename Component,
    typename Value>
concept CellBasedStorage = requires(
    T storage,
    const T const_storage,
    typename T::CellIndex index,
    Value value,
    const glm::vec<n_dims, Component> point,
    const glm::vec<n_dims, int32_t> offset) {
    typename T::CellIndex;

    { const_storage.point_to_cell_index(point) } -> std::same_as<typename T::CellIndex>;
    { const_storage.offset_cell_index(index, offset) } -> std::same_as<typename T::CellIndex>;
    { const_storage.cell_bounds(index) } -> std::same_as<radix::geometry::Aabb<n_dims, Component>>;

    { storage.insert(point, value) } -> std::same_as<bool>;

    {
        storage.for_all_in_cell(index, [](const glm::vec<n_dims, Component>&, Value&) {})
    } -> std::same_as<bool>;
    {
        const_storage.for_all_in_cell(index, [](const glm::vec<n_dims, Component>&, const Value&) {})
    } -> std::same_as<bool>;

    {
        storage.for_all_points([](const glm::vec<n_dims, Component> &, Value &) {})
    } -> std::same_as<bool>;

    {
        const_storage.for_all_points([](const glm::vec<n_dims, Component> &, const Value &) {})
    } -> std::same_as<bool>;
};

} // namespace spatial_lookup
