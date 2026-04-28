#pragma once

#include <libassert/assert.hpp>

#include "spatial_lookup/SpatialLookup.h"
#include "mesh/merging/VertexDeduplicate.h"

namespace mesh::merging {
    
template <glm::length_t n_dims, typename Component, typename Meta, spatial_lookup::SpatialLookup<n_dims, Component, Meta> Lookup>
class EpsilonVertexDeduplicate : public VertexDeduplicate<n_dims, Component, Meta> {
public:
    using Vec = glm::vec<n_dims, Component>;

    EpsilonVertexDeduplicate(Lookup lookup, Component epsilon)
        : _lookup(std::move(lookup)),
          _epsilon(epsilon) {}
          
    Component epsilon() const {
        return this->_epsilon;
    }
    virtual void insert(const Vec &point, const Meta meta) override {
        DEBUG_ASSERT_VAL(this->_lookup.insert(point, meta));
    }
    virtual bool find(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        return this->_lookup.find_all_near(point, this->_epsilon, duplicates);
    }

private:
    Lookup _lookup;
    Component _epsilon;
};

template <typename L,
          typename C = typename L::Vec::value_type,
          typename M = typename L::Value,
          glm::length_t N = L::Vec::length()>
EpsilonVertexDeduplicate(L lookup, C epsilon)
    -> EpsilonVertexDeduplicate<N, C, M, L>;

}
