#pragma once

#include <filesystem>
#include <vector>

#include "io/envelope_file.h"
#include "mesh/SimpleMesh.h"
#include "mesh/codec/SfMeshFormat.h"
#include "store/Codec.h"

namespace mesh::codec {

class SfMesh final : public store::Codec<mesh::Simple> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, ".sfmesh") };
    }

    std::expected<mesh::Simple, ::Error> read(const std::filesystem::path& node_path) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        auto payload = ::io::envelope::read_from_path<mesh::sf::Schema>(path);
        if (!payload) {
            return std::unexpected(std::move(payload).error().with_context("read SF mesh"));
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return std::unexpected(
                ::Error::make(::Error::Code::CorruptData, "invalid SF mesh in \"" + path.string() + "\": " + valid.error()));
        }
        auto decoded = mesh::sf::decode_payload(std::move(*payload));
        if (!decoded) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, "could not decode SF mesh \"" + path.string() + "\""));
        }
        return std::move(*decoded);
    }

    std::expected<void, ::Error> write(const std::filesystem::path& node_path, const mesh::Simple& mesh) const override
    {
        return write(node_path, mesh, {});
    }

    std::expected<void, ::Error> write(const std::filesystem::path& node_path, const mesh::Simple& mesh, const mesh::EncodeOptions options) const
    {
        const std::filesystem::path path = paths(node_path).front();
        auto payload = mesh::sf::encode_payload(mesh, options);
        if (!payload) {
            return std::unexpected(::Error::make(::Error::Code::InvalidInput, "could not encode SF mesh \"" + path.string() + "\""));
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return std::unexpected(
                ::Error::make(::Error::Code::InvalidInput, "invalid SF mesh for \"" + path.string() + "\": " + valid.error()));
        }
        auto result = ::io::envelope::write_to_path<mesh::sf::Schema>(*payload, path);
        if (!result) {
            return std::unexpected(std::move(result).error().with_context("write SF mesh"));
        }
        return {};
    }
};

} // namespace mesh::codec
