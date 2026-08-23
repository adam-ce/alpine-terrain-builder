#pragma once

#include <exception>
#include <filesystem>
#include <string_view>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/io/gltf.h"
#include "store/Codec.h"
#include "store/codec/Path.h"

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

    std::vector<std::filesystem::path> paths(const store::NodePath& node_path) const override
    {
        return { store::codec::append_extension(node_path, m_container == GltfContainer::Binary ? ".glb" : ".gltf") };
    }

    std::expected<mesh::Simple, store::CodecError> read(const store::NodePath& node_path) const override
    {
        const auto result = mesh::io::gltf::load_from_path(paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                result.error() == mesh::io::LoadMeshErrorKind::FileNotFound ? store::CodecErrorCategory::FileNotFound : store::CodecErrorCategory::InvalidData,
                result.error().description(),
            });
        }
        return result.value();
    }

    std::expected<void, store::CodecError> write(const store::NodePath& node_path, const mesh::Simple& mesh) const override
    {
        try {
            const auto result = mesh::io::gltf::save_to_path(mesh, paths(node_path).front());
            if (!result.has_value()) {
                return std::unexpected(store::CodecError {
                    store::CodecOperation::Write,
                    store::CodecErrorCategory::Domain,
                    result.error().description(),
                });
            }
            return {};
        } catch (const std::exception& error) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                error.what(),
            });
        } catch (...) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                "unknown glTF writer exception",
            });
        }
    }

    GltfContainer container() const { return m_container; }

private:
    GltfContainer m_container;
};

} // namespace mesh::codec
