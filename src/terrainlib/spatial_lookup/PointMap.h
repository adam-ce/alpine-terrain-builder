#pragma once

#include <optional>

#include <glm/common.hpp>

namespace spatial_lookup {

// Maps spatial positions to a single canonical Value per quantized cell.
// The first value inserted for a given location wins; subsequent inserts of
// equivalent positions return that canonical value without overwriting it.
// "Equivalent" is defined by the concrete implementation (exact equality,
// epsilon radius, quantized bucket, etc.).
template <glm::length_t n_dims, typename Component, typename Value>
class PointMap {
public:
    using Vec = glm::vec<n_dims, Component>;

    virtual ~PointMap() = default;

    // If an equivalent point already exists, return its value.
    // Otherwise insert (point -> value) and return value.
    virtual Value find_or_insert(const Vec &point, Value value) = 0;

    // Returns the stored value for an equivalent point, or nullopt if none exists.
    virtual std::optional<Value> find(const Vec &point) const = 0;
};

} // namespace spatial_lookup
