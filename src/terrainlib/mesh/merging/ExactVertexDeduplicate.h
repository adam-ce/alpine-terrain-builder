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

    virtual void insert(const Vec &point, Meta meta) override {
        DEBUG_ASSERT_VAL(this->_lookup.insert(point, std::move(meta)));
    }
    virtual bool find(const Vec &point, std::vector<Meta> &matches) const override {
        return this->_lookup.find_all_at(point, matches);
    }

private:
    Lookup _lookup;
};

template <typename L,
          typename C = typename L::Vec::value_type,
          typename M = typename L::Value,
          glm::length_t N = L::Vec::length()>
ExactVertexDeduplicate(L lookup)
    -> ExactVertexDeduplicate<N, C, M, L>;

} // namespace mesh::merging
