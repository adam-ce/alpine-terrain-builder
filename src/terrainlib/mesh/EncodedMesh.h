/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include <glm/glm.hpp>
namespace mesh {

class Encoded {
public:
    struct Header {
        uint32_t version;
        uint32_t n_dims;
        uint32_t component_type;
        uint32_t vertex_count;
        uint32_t face_count;

        bool operator==(const Header &) const = default;
        bool operator!=(const Header &) const = default;
    };

    Header header;
    std::vector<uint8_t> triangles;
    std::vector<uint8_t> positions;
    std::vector<uint8_t> uvs;
    std::vector<uint8_t> texture;
};

} // namespace mesh
