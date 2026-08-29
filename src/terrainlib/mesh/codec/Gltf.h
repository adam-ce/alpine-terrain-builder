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
