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

    std::expected<mesh::Simple, ::Error> read(const std::filesystem::path& node_path) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        const auto result = mesh::io::gltf::load_from_path(path);
        if (!result.has_value()) {
            switch (static_cast<mesh::io::LoadMeshErrorKind>(result.error())) {
            case mesh::io::LoadMeshErrorKind::UnsupportedFormat:
                return std::unexpected(::Error::make(::Error::Code::Unsupported, "load unsupported glTF format from", path));
            case mesh::io::LoadMeshErrorKind::FileNotFound:
                return std::unexpected(::Error::make(::Error::Code::NotFound, "open glTF file", path));
            case mesh::io::LoadMeshErrorKind::InvalidFormat:
                return std::unexpected(::Error::make(::Error::Code::CorruptData, "decode invalid glTF file", path));
            case mesh::io::LoadMeshErrorKind::OutOfMemory:
                return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "load glTF file: out of memory", path));
            }
            std::terminate();
        }
        return std::move(result.value());
    }

    std::expected<void, ::Error> write(const std::filesystem::path& node_path, const mesh::Simple& mesh) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        try {
            const auto result = mesh::io::gltf::save_to_path(mesh, path);
            if (!result.has_value()) {
                switch (static_cast<mesh::io::SaveMeshErrorKind>(result.error())) {
                case mesh::io::SaveMeshErrorKind::UnsupportedFormat:
                    return std::unexpected(::Error::make(::Error::Code::Unsupported, "write unsupported glTF format to", path));
                case mesh::io::SaveMeshErrorKind::OpenFile:
                    return std::unexpected(::Error::make(::Error::Code::Io, "open glTF file for writing", path));
                case mesh::io::SaveMeshErrorKind::WriteFile:
                    return std::unexpected(::Error::make(::Error::Code::Io, "write glTF file", path));
                case mesh::io::SaveMeshErrorKind::OutOfMemory:
                    return std::unexpected(::Error::make(::Error::Code::ResourceExhausted, "write glTF file: out of memory", path));
                }
                std::terminate();
            }
            return {};
        } catch (const std::bad_alloc&) {
            throw;
        } catch (const std::exception& error) {
            return std::unexpected(::Error::make(::Error::Code::Internal, "write glTF file \"" + path.string() + "\": " + error.what()));
        } catch (...) {
            return std::unexpected(::Error::make(::Error::Code::Internal, "write glTF file \"" + path.string() + "\": unknown exception"));
        }
    }

    GltfContainer container() const { return m_container; }

private:
    GltfContainer m_container;
};

} // namespace mesh::codec
