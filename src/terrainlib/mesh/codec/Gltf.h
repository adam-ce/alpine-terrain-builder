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

#include <exception>
#include <filesystem>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/io/gltf.h"
#include "store/Codec.h"

namespace mesh::codec {

enum class GltfContainer {
    Binary,
    Json,
};

class Gltf final : public store::Codec<mesh::Simple> {
public:
    explicit Gltf(const GltfContainer container)
        : m_container(container)
    {
    }

    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, m_container == GltfContainer::Binary ? ".glb" : ".gltf") };
    }

    Expected<mesh::Simple> read(const std::filesystem::path& node_path) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        auto result = mesh::io::gltf::load_from_path(path);
        if (!result) {
            return Error::propagate(std::move(result), "read glTF mesh \"" + path.string() + "\"");
        }
        return std::move(*result);
    }

    Expected<void> write(const std::filesystem::path& node_path, const mesh::Simple& mesh) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        try {
            auto result = mesh::io::gltf::save_to_path(mesh, path);
            if (!result) {
                return Error::propagate(std::move(result), "write glTF mesh \"" + path.string() + "\"");
            }
            return {};
        } catch (const std::bad_alloc&) {
            return Error::fail(Error::Code::ResourceExhausted, "write glTF mesh \"" + path.string() + "\": out of memory");
        } catch (const std::exception& error) {
            return Error::fail(Error::Code::Internal, "write glTF mesh \"" + path.string() + "\": " + error.what());
        } catch (...) {
            return Error::fail(Error::Code::Internal, "write glTF mesh \"" + path.string() + "\": unknown exception");
        }
    }

    GltfContainer container() const { return m_container; }

private:
    GltfContainer m_container;
};

} // namespace mesh::codec
