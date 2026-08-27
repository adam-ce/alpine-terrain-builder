#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "codec/DagFormat.h"
#include "io/envelope_file.h"
#include "store/Codec.h"

namespace dag::codec {

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
            return std::unexpected(std::move(clustering_payload).error());
        }
        auto metadata_payload = io::envelope::read_from_path<dag::format::MetadataSchema>(node_paths[1]);
        if (!metadata_payload) {
            return std::unexpected(std::move(metadata_payload).error());
        }
        auto clustering = dag::format::decode_clustering(std::move(*clustering_payload));
        if (!clustering) {
            return std::unexpected(::Error::make(
                ::Error::Code::CorruptData, "invalid DAG clustering in \"" + node_paths[0].string() + "\": " + clustering.error()));
        }
        auto metadata = dag::format::decode_metadata(std::move(*metadata_payload));
        if (!metadata) {
            return std::unexpected(::Error::make(
                ::Error::Code::CorruptData, "invalid DAG metadata in \"" + node_paths[1].string() + "\": " + metadata.error()));
        }
        dag::ClusterBatch batch {
            .metadata = std::move(*metadata),
            .clustering = std::move(*clustering),
        };
        if (auto valid = validate_batch(batch, ::Error::Code::CorruptData); !valid) {
            return std::unexpected(std::move(valid).error().with_context(
                "validate DAG files \"" + node_paths[0].string() + "\" and \"" + node_paths[1].string() + "\""));
        }
        return batch;
    }

    std::expected<void, ::Error> write(const std::filesystem::path& node_path, const dag::ClusterBatch& batch) const override
    {
        const auto node_paths = paths(node_path);
        if (auto valid = validate_batch(batch, ::Error::Code::InvalidInput); !valid) {
            return std::unexpected(std::move(valid).error().with_context(
                "validate DAG files \"" + node_paths[0].string() + "\" and \"" + node_paths[1].string() + "\""));
        }
        auto clustering = dag::format::encode_clustering(batch.clustering);
        if (!clustering) {
            return std::unexpected(::Error::make(
                ::Error::Code::InvalidInput, "cannot encode DAG clustering for \"" + node_paths[0].string() + "\": " + clustering.error()));
        }
        const dag::format::NodeMetadata metadata = dag::format::encode_metadata(batch.metadata);
        if (auto valid = dag::format::validate(*clustering); !valid) {
            return std::unexpected(::Error::make(
                ::Error::Code::InvalidInput, "invalid DAG clustering for \"" + node_paths[0].string() + "\": " + valid.error()));
        }
        if (auto valid = dag::format::validate(metadata); !valid) {
            return std::unexpected(::Error::make(
                ::Error::Code::InvalidInput, "invalid DAG metadata for \"" + node_paths[1].string() + "\": " + valid.error()));
        }
        auto data_written = io::envelope::write_to_path<dag::format::ClusteringSchema>(*clustering, node_paths[0]);
        if (!data_written) {
            return data_written;
        }
        auto metadata_written = io::envelope::write_to_path<dag::format::MetadataSchema>(metadata, node_paths[1]);
        if (!metadata_written) {
            return metadata_written;
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
        const std::filesystem::path path = paths(node_path).front();
        auto payload = io::envelope::read_from_path<dag::format::MetadataSchema>(path);
        if (!payload) {
            return std::unexpected(std::move(payload).error());
        }
        auto metadata = dag::format::decode_metadata(std::move(*payload));
        if (!metadata) {
            return std::unexpected(
                ::Error::make(::Error::Code::CorruptData, "invalid DAG metadata in \"" + path.string() + "\": " + metadata.error()));
        }
        return std::move(*metadata);
    }
};

} // namespace dag::codec
