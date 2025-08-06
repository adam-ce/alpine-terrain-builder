#pragma once

#include <optional>
#include <vector>

#include <glm/common.hpp>

namespace spatial_lookup {

template <glm::length_t n_dims, typename Component, typename Value>
class SpatialLookup {
public:
    using Vec = glm::vec<n_dims, Component>;
    using ForAllNear = std::function<void(const Vec&, Value &, const Component)>;
    using ForAllNearConst = std::function<void(const Vec&, const Value &, const Component)>;

    virtual void clear() = 0;
    virtual void insert(const Vec &point, const Value value) = 0;

    virtual std::optional<std::reference_wrapper<Value>> find_nearest(const Vec &point, const Component epsilon) = 0;
    virtual std::optional<std::reference_wrapper<const Value>> find_nearest(const Vec &point, const Component epsilon) const = 0;

    virtual bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<Value>> &out) = 0;
    virtual bool find_all_near(const Vec &point, const Component epsilon, std::vector<std::reference_wrapper<const Value>> &out) const = 0;

    virtual void for_all_near(const Vec &point, const Component epsilon, ForAllNear func) = 0;
    virtual void for_all_near(const Vec &point, const Component epsilon, ForAllNearConst func) const = 0;
};

} // namespace spatial_lookup
