#pragma once

#include <filesystem>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/io/terrain.h"
#include "store/Codec.h"
#include "store/codec/ZppBits.h"

namespace mesh::codec {

class Terrain final : public store::Codec<mesh::Simple> {
public:
    std::vector<std::filesystem::path> paths(const store::NodePath &node_path) const override {
        return {store::codec::append_extension(node_path, ".terrain")};
    }

    std::expected<mesh::Simple, store::CodecError> read(
        const store::NodePath &node_path) const override {
        const auto result = mesh::io::terrain::load_from_path(paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Read,
                result.error() == mesh::io::LoadMeshErrorKind::FileNotFound
                    ? store::CodecErrorCategory::FileNotFound
                    : store::CodecErrorCategory::InvalidData,
                result.error().description(),
            });
        }
        return result.value();
    }

    std::expected<void, store::CodecError> write(
        const store::NodePath &node_path,
        const mesh::Simple &mesh) const override {
        const auto result = mesh::io::terrain::save_to_path(mesh, paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                result.error().description(),
            });
        }
        return {};
    }
};

} // namespace mesh::codec
