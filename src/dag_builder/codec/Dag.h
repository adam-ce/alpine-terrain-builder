#pragma once

#include <filesystem>
#include <vector>

#include "codec/DagFormat.h"
#include "io/envelope_file.h"
#include "store/Codec.h"
#include "store/codec/Path.h"

namespace dag::codec {

inline store::CodecError file_error(
    const store::CodecOperation operation,
    const io::envelope::FileError &error)
{
    const bool missing = std::holds_alternative<io::Error>(error)
        && std::get<io::Error>(error) == io::Error(io::Error::OpenFile);
    return {
        operation,
        missing ? store::CodecErrorCategory::FileNotFound
                : operation == store::CodecOperation::Read
                    ? store::CodecErrorCategory::InvalidData
                    : store::CodecErrorCategory::Io,
        io::envelope::describe_error(error),
    };
}

class Dag final : public store::Codec<dag::ClusterBatch> {
public:
    std::vector<std::filesystem::path> paths(
        const store::NodePath &node_path) const override
    {
        return {
            store::codec::append_extension(node_path, ".dag"),
            store::codec::append_extension(node_path, ".dagmeta"),
        };
    }

    std::expected<dag::ClusterBatch, store::CodecError> read(
        const store::NodePath &node_path) const override
    {
        const auto node_paths = paths(node_path);
        auto clustering_payload =
            io::envelope::read_from_path<dag::format::ClusteringSchema>(node_paths[0]);
        if (!clustering_payload) {
            return std::unexpected(file_error(
                store::CodecOperation::Read,
                clustering_payload.error()));
        }
        auto metadata_payload =
            io::envelope::read_from_path<dag::format::MetadataSchema>(node_paths[1]);
        if (!metadata_payload) {
            return std::unexpected(file_error(
                store::CodecOperation::Read,
                metadata_payload.error()));
        }
        auto clustering = dag::format::decode_clustering(std::move(*clustering_payload));
        if (!clustering) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Read,
                store::CodecErrorCategory::InvalidData,
                clustering.error(),
            });
        }
        auto metadata = dag::format::decode_metadata(std::move(*metadata_payload));
        if (!metadata) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Read,
                store::CodecErrorCategory::InvalidData,
                metadata.error(),
            });
        }
        return dag::ClusterBatch{
            .metadata = std::move(*metadata),
            .clustering = std::move(*clustering),
        };
    }

    std::expected<void, store::CodecError> write(
        const store::NodePath &node_path,
        const dag::ClusterBatch &batch) const override
    {
        auto clustering = dag::format::encode_clustering(batch.clustering);
        if (!clustering) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                clustering.error(),
            });
        }
        const dag::format::NodeMetadata metadata =
            dag::format::encode_metadata(batch.metadata);
        if (auto valid = dag::format::validate(*clustering); !valid) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                valid.error(),
            });
        }
        if (auto valid = dag::format::validate(metadata); !valid) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Write,
                store::CodecErrorCategory::Domain,
                valid.error(),
            });
        }
        const auto node_paths = paths(node_path);
        auto data_written = io::envelope::write_to_path<dag::format::ClusteringSchema>(
            *clustering,
            node_paths[0]);
        if (!data_written) {
            return std::unexpected(file_error(
                store::CodecOperation::Write,
                data_written.error()));
        }
        auto metadata_written = io::envelope::write_to_path<dag::format::MetadataSchema>(
            metadata,
            node_paths[1]);
        if (!metadata_written) {
            return std::unexpected(file_error(
                store::CodecOperation::Write,
                metadata_written.error()));
        }
        return {};
    }
};

} // namespace dag::codec
