#pragma once

#include <libassert/assert.hpp>

#include "mesh/merging/EpsilonVertexDeduplicate.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "spatial_lookup/SpatialLookup.h"

namespace mesh::merging {

namespace detail {
template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> scale_to_length(const glm::vec<n_dims, T> &v, const T target_length) {
    const T len = glm::length(v);
    if (len == T(0)) {
        return glm::vec<n_dims, T>(T(0));
    }
    return v * (target_length / len);
}
}

template <typename Meta, spatial_lookup::SpatialLookup<3, double, Meta> Lookup>
class SphereProjectionVertexDeduplicate : public VertexDeduplicate<3, double, Meta> {
public:
    using Vec = glm::vec<3, double>;

    SphereProjectionVertexDeduplicate(Lookup lookup, double epsilon, double radius)
        : _inner(std::move(lookup), epsilon), _radius(radius) {}

    void insert(const Vec &point, const Meta meta) override {
        const Vec mapped = detail::scale_to_length(point, this->_radius);
        this->_inner.insert(mapped, meta);
    }
    bool find(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        const Vec mapped = detail::scale_to_length(point, this->_radius);
        return this->_inner.find(mapped, duplicates);
    }

private:
    EpsilonVertexDeduplicate<3, double, Meta, Lookup> _inner;
    double _radius;
};
}
