#pragma once

#include "mesh/merging/EpsilonVertexDeduplicate.h"
#include "mesh/merging/VertexDeduplicate.h"
#include "spatial_lookup/SpatialLookup.h"

namespace mesh::merging {

template <typename Meta, spatial_lookup::SpatialLookup<2, double, Meta> Lookup>
class SphereVertexDeduplicate : public VertexDeduplicate<3, double, Meta> {
public:
    using Vec = glm::vec<3, double>;
    using MappedVec = glm::vec<2, double>;

    SphereVertexDeduplicate(Lookup &lookup, double epsilon, Vec tangent_point)
        : _inner(lookup, epsilon), _projector(tangent_point) {}

    void add(const Vec &point, const Meta meta) override {
        const MappedVec mapped = this->_projector.project_point(point);
        this->_inner.add(mapped, meta);
    }
    bool get(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        const MappedVec mapped = this->_projector.project_point(point);
        return this->_inner.get(mapped, duplicates);
    }

private:
    EpsilonVertexDeduplicate<2, double, Meta, Lookup> _inner;
    SphereProjector _projector;
};

}
