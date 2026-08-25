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
    std::vector<std::filesystem::path> paths(const store::NodePath& node_path) const override
    {
        return { store::codec::append_extension(node_path, ".sfmesh") };
    }

    std::expected<mesh::Simple, store::CodecError> read(const store::NodePath& node_path) const override
    {
        auto payload = ::io::envelope::read_from_path<mesh::sf::Schema>(paths(node_path).front());
        if (!payload) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                ::io::envelope::is_file_not_found(payload.error()) ? store::CodecErrorCategory::FileNotFound : store::CodecErrorCategory::InvalidData,
                ::io::envelope::describe_error(payload.error()),
            });
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::InvalidData,
                valid.error(),
            });
        }
        auto decoded = mesh::sf::decode_payload(std::move(*payload));
        if (!decoded) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::InvalidData,
                "could not decode SF mesh payload",
            });
        }
        return std::move(*decoded);
    }

    std::expected<void, store::CodecError> write(const store::NodePath& node_path, const mesh::Simple& mesh) const override
    {
        return write(node_path, mesh, {});
    }

    std::expected<void, store::CodecError> write(const store::NodePath& node_path, const mesh::Simple& mesh, const mesh::EncodeOptions options) const
    {
        auto payload = mesh::sf::encode_payload(mesh, options);
        if (!payload) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                "could not encode SF mesh payload",
            });
        }
        if (auto valid = mesh::sf::validate(*payload); !valid) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                valid.error(),
            });
        }
        auto result = ::io::envelope::write_to_path<mesh::sf::Schema>(*payload, paths(node_path).front());
        if (!result) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Write,
                store::CodecErrorCategory::Io,
                ::io::envelope::describe_error(result.error()),
            });
        }
        return {};
    }
};

} // namespace mesh::codec
