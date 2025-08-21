#pragma once

#include "mesh/merging/EpsilonVertexDeduplicate.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "spatial_lookup/SpatialLookup.h"

namespace mesh::merging {

namespace {
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
class SphereVertexDeduplicate : public VertexDeduplicate<3, double, Meta> {
public:
    using Vec = glm::vec<3, double>;

    SphereVertexDeduplicate(Lookup &lookup, double epsilon, double radius)
        : _inner(lookup, epsilon), _radius(radius) {}

    void add(const Vec &point, const Meta meta) override {
        const Vec mapped = scale_to_length(point, this->_radius);
        this->_inner.add(mapped, meta);
    }
    bool get(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        const Vec mapped = scale_to_length(point, this->_radius);
        return this->_inner.get(mapped, duplicates);
    }

private:
    EpsilonVertexDeduplicate<3, double, Meta, Lookup> _inner;
    double _radius;
};
}
