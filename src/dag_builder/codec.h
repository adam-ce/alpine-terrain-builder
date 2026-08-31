#pragma once

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#include "dag_node.h"
#include "store/Codec.h"

namespace dag::codec {

class ClusterBatch final : public store::Codec<dag::ClusterBatch> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override;
    Expected<dag::ClusterBatch> read(const std::filesystem::path& node_path) const override;
    Expected<void> write(const std::filesystem::path& node_path, const dag::ClusterBatch& batch) const override;
};

class Metadata final : public store::Codec<dag::NodeMetadata> {
public:
    std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const override;
    Expected<dag::NodeMetadata> read(const std::filesystem::path& node_path) const override;
};

Expected<std::unique_ptr<store::Codec<dag::ClusterBatch>>> cluster_batch_from_extension(std::string_view extension);
Expected<std::unique_ptr<store::Codec<dag::NodeMetadata>>> metadata_from_extension(std::string_view extension);

} // namespace dag::codec
