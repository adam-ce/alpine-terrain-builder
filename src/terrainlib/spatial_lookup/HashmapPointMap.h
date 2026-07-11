#pragma once

#include <optional>
#include <unordered_map>

#include <glm/glm.hpp>

#include "spatial_lookup/PointMap.h"
#include "spatial_lookup/QuantizedHash.h"

namespace spatial_lookup {

// PointMap implementation backed by a quantized hashmap.
// At most one Value is stored per quantized cell; the first insert wins.
template <glm::length_t n_dims, typename Component, typename Value>
class HashmapPointMap : public PointMap<n_dims, Component, Value> {
public:
    using Vec = glm::vec<n_dims, Component>;

    explicit HashmapPointMap(Component epsilon)
        : _epsilon(epsilon), _map(0, Hash(epsilon), Equal(epsilon)) {}

    Value find_or_insert(const Vec &point, Value value) override {
        // try_emplace performs a single lookup; the first insert wins
        const auto [it, inserted] = this->_map.try_emplace(point, std::move(value));
        return it->second;
    }

    std::optional<Value> find(const Vec &point) const override {
        const auto it = this->_map.find(point);
        if (it == this->_map.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void clear() {
        this->_map.clear();
    }

    [[nodiscard]] bool empty() const {
        return this->_map.empty();
    }

    [[nodiscard]] size_t size() const {
        return this->_map.size();
    }

    Component epsilon() const {
        return this->_epsilon;
    }

private:
    using Hash = detail::QuantizedVecHash<n_dims, Component>;
    using Equal = detail::QuantizedVecEqual<n_dims, Component>;

    Component _epsilon;
    std::unordered_map<Vec, Value, Hash, Equal> _map;
};

} // namespace spatial_lookup
