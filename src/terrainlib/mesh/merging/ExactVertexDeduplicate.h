#pragma once

#include <libassert/assert.hpp>

#include "spatial_lookup/SpatialLookup.h"
#include "mesh/merging/VertexDeduplicate.h"

namespace mesh::merging {

template <glm::length_t n_dims, typename Component, typename Meta, spatial_lookup::SpatialLookup<n_dims, Component, Meta> Lookup>
class ExactVertexDeduplicate : public VertexDeduplicate<n_dims, Component, Meta> {
public:
    using Vec = glm::vec<n_dims, Component>;

    ExactVertexDeduplicate(Lookup lookup)
        : _lookup(std::move(lookup)) {}
          
    virtual void insert(const Vec &point, const Meta meta) override {
        DEBUG_ASSERT_VAL(this->_lookup.insert(point, meta));
    }
    virtual bool find(const Vec &point, std::vector<std::reference_wrapper<const Meta>> &duplicates) const override {
        return this->_lookup.find_all_at(point, duplicates);
    }

private:
    Lookup _lookup;
    Component _epsilon;
};

template <typename L,
          typename M = typename L::Value,
          glm::length_t N = L::Vec::length()>
ExactVertexDeduplicate(L lookup)
    -> ExactVertexDeduplicate<N, M, L>;
}
