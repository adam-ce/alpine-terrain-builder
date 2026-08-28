#pragma once

#include <exception>
#include <filesystem>
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
        const auto result = mesh::io::gltf::load_from_path(path);
        if (!result.has_value()) {
            switch (static_cast<mesh::io::LoadMeshErrorKind>(result.error())) {
            case mesh::io::LoadMeshErrorKind::UnsupportedFormat:
                return Error::fail(Error::Code::Unsupported, "load unsupported glTF format from", path);
            case mesh::io::LoadMeshErrorKind::FileNotFound:
                return Error::fail(Error::Code::NotFound, "open glTF file", path);
            case mesh::io::LoadMeshErrorKind::InvalidFormat:
                return Error::fail(Error::Code::CorruptData, "decode invalid glTF file", path);
            case mesh::io::LoadMeshErrorKind::OutOfMemory:
                return Error::fail(Error::Code::ResourceExhausted, "load glTF file: out of memory", path);
            }
            std::terminate();
        }
        return std::move(result.value());
    }

    Expected<void> write(const std::filesystem::path& node_path, const mesh::Simple& mesh) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        try {
            const auto result = mesh::io::gltf::save_to_path(mesh, path);
            if (!result.has_value()) {
                switch (static_cast<mesh::io::SaveMeshErrorKind>(result.error())) {
                case mesh::io::SaveMeshErrorKind::UnsupportedFormat:
                    return Error::fail(Error::Code::Unsupported, "write unsupported glTF format to", path);
                case mesh::io::SaveMeshErrorKind::OpenFile:
                    return Error::fail(Error::Code::Io, "open glTF file for writing", path);
                case mesh::io::SaveMeshErrorKind::WriteFile:
                    return Error::fail(Error::Code::Io, "write glTF file", path);
                case mesh::io::SaveMeshErrorKind::OutOfMemory:
                    return Error::fail(Error::Code::ResourceExhausted, "write glTF file: out of memory", path);
                }
                std::terminate();
            }
            return {};
        } catch (const std::exception& error) {
            return Error::fail(Error::Code::Internal, "write glTF file \"" + path.string() + "\": " + error.what());
        } catch (...) {
            return Error::fail(Error::Code::Internal, "write glTF file \"" + path.string() + "\": unknown exception");
        }
    }

    GltfContainer container() const { return m_container; }

private:
    GltfContainer m_container;
};

} // namespace mesh::codec
