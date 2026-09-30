/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
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

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "mesh/codec/Gltf.h"
#include "mesh/codec/SfMesh.h"
#include "store/Codec.h"

namespace mesh::codec {

inline Expected<std::unique_ptr<store::Codec<mesh::Simple>>> from_extension(const std::string_view extension)
{
    if (extension == ".sfmesh") {
        return std::make_unique<SfMesh>();
    }
    if (extension == ".glb") {
        return std::make_unique<Gltf>(GltfContainer::Binary);
    }
    if (extension == ".gltf") {
        return std::make_unique<Gltf>(GltfContainer::Json);
    }
    return Error::fail(Error::Code::Unsupported, "unsupported mesh codec selector: " + std::string(extension));
}

} // namespace mesh::codec
