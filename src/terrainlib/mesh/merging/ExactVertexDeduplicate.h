/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
