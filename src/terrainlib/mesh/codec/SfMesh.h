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

    Expected<mesh::Simple> read(const std::filesystem::path& node_path) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        auto payload = ::io::envelope::read_from_path<mesh::sf::Schema>(path);
        if (!payload) {
            return Error::propagate(std::move(payload), "read SF mesh");
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return Error::propagate(
                std::move(valid), Error::Code::CorruptData, "validate SF mesh read from \"" + path.string() + "\"");
        }
        auto decoded = mesh::sf::decode_payload(std::move(*payload));
        if (!decoded) {
            return Error::propagate(std::move(decoded), "decode SF mesh \"" + path.string() + "\"");
        }
        return std::move(*decoded);
    }

    Expected<void> write(const std::filesystem::path& node_path, const mesh::Simple& mesh) const override
    {
        return write(node_path, mesh, {});
    }

    Expected<void> write(const std::filesystem::path& node_path, const mesh::Simple& mesh, const mesh::EncodeOptions options) const
    {
        const std::filesystem::path path = paths(node_path).front();
        auto payload = mesh::sf::encode_payload(mesh, options);
        if (!payload) {
            return Error::propagate(std::move(payload), "encode SF mesh \"" + path.string() + "\"");
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return Error::propagate(std::move(valid), "validate SF mesh for \"" + path.string() + "\"");
        }
        auto result = ::io::envelope::write_to_path<mesh::sf::Schema>(*payload, path);
        if (!result) {
            return Error::propagate(std::move(result), "write SF mesh");
        }
        return {};
    }
};

} // namespace mesh::codec
