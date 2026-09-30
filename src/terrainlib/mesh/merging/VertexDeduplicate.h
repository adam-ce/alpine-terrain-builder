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

#include <vector>

#include <glm/common.hpp>

namespace mesh::merging {

template <glm::length_t n_dims, typename Component, typename Meta>
class VertexDeduplicate {
public:
    using Vec = glm::vec<n_dims, Component>;
    using Matches = std::vector<Meta>;

    virtual ~VertexDeduplicate() = default;

    virtual void insert(const Vec& point, Meta meta) = 0;
    // Appends all matches for point to `matches`. Returns true if any were found.
    // Values are returned by copy to avoid dangling references into the backing store.
    virtual bool find(const Vec& point, Matches &matches) const = 0;
    virtual bool find_or_insert(const Vec &point, Meta meta, Matches &matches) {
        if (this->find(point, matches)) {
            return false;
        } else {
            this->insert(point, std::move(meta));
            return true;
        }
    }
    Matches find(const Vec &point) const {
        Matches matches;
        this->find(point, matches);
        return matches;
    }
};

} // namespace mesh::merging
