#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "codec/DagFormat.h"
#include "io/envelope_file.h"
#include "store/Codec.h"

namespace dag::codec {

inline Expected<void> validate_batch(const dag::ClusterBatch& batch, const Error::Code code)
{
    if (batch.metadata.group_assignment.size() != batch.clustering.clusters.size()) {
        return Error::fail(code, "DAG metadata group-assignment count does not match clustering cluster count");
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

    Expected<dag::ClusterBatch> read(const std::filesystem::path& node_path) const override
    {
        const auto node_paths = paths(node_path);
        auto clustering_payload = io::envelope::read_from_path<dag::format::ClusteringSchema>(node_paths[0]);
        if (!clustering_payload) {
            return Error::propagate(std::move(clustering_payload), "read DAG clustering");
        }
        auto metadata_payload = io::envelope::read_from_path<dag::format::MetadataSchema>(node_paths[1]);
        if (!metadata_payload) {
            return Error::propagate(std::move(metadata_payload), "read DAG metadata");
        }
        auto clustering = dag::format::decode_clustering(std::move(*clustering_payload));
        if (!clustering) {
            return Error::fail(
                Error::Code::CorruptData, "invalid DAG clustering in \"" + node_paths[0].string() + "\": " + clustering.error());
        }
        auto metadata = dag::format::decode_metadata(std::move(*metadata_payload));
        if (!metadata) {
            return Error::fail(
                Error::Code::CorruptData, "invalid DAG metadata in \"" + node_paths[1].string() + "\": " + metadata.error());
        }
        dag::ClusterBatch batch {
            .metadata = std::move(*metadata),
            .clustering = std::move(*clustering),
        };
        if (auto valid = validate_batch(batch, Error::Code::CorruptData); !valid) {
            return Error::propagate(std::move(valid),
                "validate DAG files \"" + node_paths[0].string() + "\" and \"" + node_paths[1].string() + "\"");
        }
        return batch;
    }

    Expected<void> write(const std::filesystem::path& node_path, const dag::ClusterBatch& batch) const override
    {
        const auto node_paths = paths(node_path);
        if (auto valid = validate_batch(batch, Error::Code::InvalidInput); !valid) {
            return Error::propagate(std::move(valid),
                "validate DAG files \"" + node_paths[0].string() + "\" and \"" + node_paths[1].string() + "\"");
        }
        auto clustering = dag::format::encode_clustering(batch.clustering);
        if (!clustering) {
            return Error::fail(
                Error::Code::InvalidInput, "cannot encode DAG clustering for \"" + node_paths[0].string() + "\": " + clustering.error());
        }
        const dag::format::NodeMetadata metadata = dag::format::encode_metadata(batch.metadata);
        if (auto valid = dag::format::validate(*clustering); !valid) {
            return Error::fail(
                Error::Code::InvalidInput, "invalid DAG clustering for \"" + node_paths[0].string() + "\": " + valid.error());
        }
        if (auto valid = dag::format::validate(metadata); !valid) {
            return Error::fail(
                Error::Code::InvalidInput, "invalid DAG metadata for \"" + node_paths[1].string() + "\": " + valid.error());
        }
        auto data_written = io::envelope::write_to_path<dag::format::ClusteringSchema>(*clustering, node_paths[0]);
        if (!data_written) {
            return Error::propagate(std::move(data_written), "write DAG clustering");
        }
        auto metadata_written = io::envelope::write_to_path<dag::format::MetadataSchema>(metadata, node_paths[1]);
        if (!metadata_written) {
            return Error::propagate(std::move(metadata_written), "write DAG metadata");
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

    Expected<dag::NodeMetadata> read(const std::filesystem::path& node_path) const override
    {
        const std::filesystem::path path = paths(node_path).front();
        auto payload = io::envelope::read_from_path<dag::format::MetadataSchema>(path);
        if (!payload) {
            return Error::propagate(std::move(payload), "read DAG metadata");
        }
        auto metadata = dag::format::decode_metadata(std::move(*payload));
        if (!metadata) {
            return Error::fail(Error::Code::CorruptData, "invalid DAG metadata in \"" + path.string() + "\": " + metadata.error());
        }
        return std::move(*metadata);
    }
};

} // namespace dag::codec
