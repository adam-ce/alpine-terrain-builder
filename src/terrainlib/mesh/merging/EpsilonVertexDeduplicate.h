#pragma once

#include "spatial_lookup/SpatialLookup.h"
#include "mesh/merging/VertexDeduplicate.h"

namespace mesh::merging {
    
template <glm::length_t n_dims, typename Component, typename Meta, spatial_lookup::SpatialLookup<n_dims, Component, Meta> Lookup>
class EpsilonVertexDeduplicate : public VertexDeduplicate<n_dims, Component, Meta> {
public:
    using Vec = glm::vec<n_dims, Component>;

    EpsilonVertexDeduplicate(Lookup &lookup, Component epsilon)
        : _lookup(lookup),
          _epsilon(epsilon) {}

    void add(const Vec &point, const Meta meta) override {
        this->_lookup.insert(point, meta);
    }
    bool get(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        return this->_lookup.find_all_near(point, this->_epsilon, duplicates);
    }

private:
    Lookup &_lookup;
    Component _epsilon;
};

// This deduction guide is placed after your class definition
template <glm::length_t N, typename C, typename M, typename S,
          template <glm::length_t, typename, typename, typename> class CellBased,
          typename EpsilonType>
EpsilonVertexDeduplicate(CellBased<N, C, M, S> &lookup, EpsilonType epsilon)
    -> EpsilonVertexDeduplicate<N, C, M, CellBased<N, C, M, S>>;

}
