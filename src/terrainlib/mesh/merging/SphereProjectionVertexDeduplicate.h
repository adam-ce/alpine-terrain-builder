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

    double radius() const {
        return this->_radius;
    }
    double epsilon() const {
        return this->_inner.epsilon();
    }

    void insert(const Vec &point, Meta meta) override {
        const Vec mapped = detail::scale_to_length(point, this->_radius);
        this->_inner.insert(mapped, std::move(meta));
    }
    bool find(const Vec &point, std::vector<Meta> &matches) const override {
        const Vec mapped = detail::scale_to_length(point, this->_radius);
        return this->_inner.find(mapped, matches);
    }

private:
    EpsilonVertexDeduplicate<3, double, Meta, Lookup> _inner;
    double _radius;
};

} // namespace mesh::merging
