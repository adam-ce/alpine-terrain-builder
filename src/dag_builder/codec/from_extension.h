#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "codec/Dag.h"
#include "dag_node.h"

namespace dag::codec {

inline Expected<std::unique_ptr<store::Codec<dag::ClusterBatch>>> from_extension(const std::string_view extension)
{
    if (extension == ".dag") {
        return std::make_unique<Dag>();
    }
    return Error::fail(Error::Code::Unsupported, "unsupported DAG codec selector: " + std::string(extension));
}

inline Expected<std::unique_ptr<store::Codec<dag::NodeMetadata>>> metadata_from_extension(const std::string_view extension)
{
    if (extension == ".dag") {
        return std::make_unique<MetadataView>();
    }
    return Error::fail(Error::Code::Unsupported, "unsupported DAG metadata codec selector: " + std::string(extension));
}

} // namespace dag::codec
