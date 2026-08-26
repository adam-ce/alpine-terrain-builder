#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "codec/DagFormat.h"
#include "io/envelope_file.h"
#include "store/Codec.h"

namespace dag::codec {

inline ::Error file_error(std::string operation, ::Error error)
{
    return std::move(error).with_context(std::move(operation));
}

inline std::expected<void, ::Error> validate_batch(const dag::ClusterBatch& batch, const ::Error::Code code)
{
    if (batch.metadata.group_assignment.size() != batch.clustering.clusters.size()) {
        return std::unexpected(::Error::make(code, "DAG metadata group-assignment count does not match clustering cluster count"));
    }
    return {};
}

class Dag final : public store::Codec<dag::ClusterBatch> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return {
            add_extension(node_path, ".dag"),
            add_extension(node_path, ".dagmeta"),
        };
    }

    std::expected<dag::ClusterBatch, ::Error> read(const std::filesystem::path& node_path) const override
    {
        const auto node_paths = paths(node_path);
        auto clustering_payload = io::envelope::read_from_path<dag::format::ClusteringSchema>(node_paths[0]);
        if (!clustering_payload) {
            return std::unexpected(file_error("read DAG clustering", std::move(clustering_payload).error()));
        }
        auto metadata_payload = io::envelope::read_from_path<dag::format::MetadataSchema>(node_paths[1]);
        if (!metadata_payload) {
            return std::unexpected(file_error("read DAG metadata", std::move(metadata_payload).error()));
        }
        auto clustering = dag::format::decode_clustering(std::move(*clustering_payload));
        if (!clustering) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, clustering.error()));
        }
        auto metadata = dag::format::decode_metadata(std::move(*metadata_payload));
        if (!metadata) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, metadata.error()));
        }
        dag::ClusterBatch batch {
            .metadata = std::move(*metadata),
            .clustering = std::move(*clustering),
        };
        if (auto valid = validate_batch(batch, ::Error::Code::CorruptData); !valid) {
            return std::unexpected(valid.error());
        }
        return batch;
    }

    std::expected<void, ::Error> write(const std::filesystem::path& node_path, const dag::ClusterBatch& batch) const override
    {
        if (auto valid = validate_batch(batch, ::Error::Code::InvalidInput); !valid) {
            return valid;
        }
        auto clustering = dag::format::encode_clustering(batch.clustering);
        if (!clustering) {
            return std::unexpected(::Error::make(::Error::Code::InvalidInput, clustering.error()));
        }
        const dag::format::NodeMetadata metadata = dag::format::encode_metadata(batch.metadata);
        if (auto valid = dag::format::validate(*clustering); !valid) {
            return std::unexpected(::Error::make(::Error::Code::InvalidInput, valid.error()));
        }
        if (auto valid = dag::format::validate(metadata); !valid) {
            return std::unexpected(::Error::make(::Error::Code::InvalidInput, valid.error()));
        }
        const auto node_paths = paths(node_path);
        auto data_written = io::envelope::write_to_path<dag::format::ClusteringSchema>(*clustering, node_paths[0]);
        if (!data_written) {
            return std::unexpected(file_error("write DAG clustering", std::move(data_written).error()));
        }
        auto metadata_written = io::envelope::write_to_path<dag::format::MetadataSchema>(metadata, node_paths[1]);
        if (!metadata_written) {
            return std::unexpected(file_error("write DAG metadata", std::move(metadata_written).error()));
        }
        return {};
    }
};

class MetadataView final : public store::Codec<dag::NodeMetadata> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override
    {
        return { add_extension(node_path, ".dagmeta") };
    }

    std::expected<dag::NodeMetadata, ::Error> read(const std::filesystem::path& node_path) const override
    {
        auto payload = io::envelope::read_from_path<dag::format::MetadataSchema>(paths(node_path).front());
        if (!payload) {
            return std::unexpected(file_error("read DAG metadata", std::move(payload).error()));
        }
        auto metadata = dag::format::decode_metadata(std::move(*payload));
        if (!metadata) {
            return std::unexpected(::Error::make(::Error::Code::CorruptData, metadata.error()));
        }
        return std::move(*metadata);
    }
};

} // namespace dag::codec
