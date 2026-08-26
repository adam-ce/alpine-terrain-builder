#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "codec/Dag.h"
#include "dag_node.h"

namespace dag::codec {

inline std::expected<std::unique_ptr<store::Codec<dag::ClusterBatch>>, ::Error> from_extension(const std::string_view extension)
{
    if (extension == ".dag") {
        return std::make_unique<Dag>();
    }
    return std::unexpected(::Error::make(::Error::Code::Unsupported, "unsupported DAG codec selector: " + std::string(extension)));
}

inline std::expected<std::unique_ptr<store::Codec<dag::NodeMetadata>>, ::Error> metadata_from_extension(const std::string_view extension)
{
    if (extension == ".dag") {
        return std::make_unique<MetadataView>();
    }
    return std::unexpected(::Error::make(::Error::Code::Unsupported, "unsupported DAG metadata codec selector: " + std::string(extension)));
}

} // namespace dag::codec
