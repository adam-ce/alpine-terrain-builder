#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <expected>

#include "codec/MetadataView.h"
#include "dag_node.h"
#include "serialization.h"
#include "store/codec/ZppBits.h"

namespace dag::codec {

inline std::expected<std::unique_ptr<store::Codec<dag::ClusterBatch>>, store::CodecError>
from_extension(const std::string_view extension) {
    if (extension == ".bin") {
        return std::make_unique<store::codec::ZppBits<dag::ClusterBatch>>();
    }
    return std::unexpected(store::CodecError{
        store::CodecOperation::Resolve,
        store::CodecErrorCategory::UnsupportedCodec,
        "unsupported DAG codec selector: " + std::string(extension),
    });
}

inline std::expected<std::unique_ptr<store::Codec<dag::NodeMetadata>>, store::CodecError>
metadata_from_extension(const std::string_view extension) {
    if (extension == ".bin") {
        return std::make_unique<MetadataView>();
    }
    return std::unexpected(store::CodecError{
        store::CodecOperation::Resolve,
        store::CodecErrorCategory::UnsupportedCodec,
        "unsupported DAG metadata codec selector: " + std::string(extension),
    });
}

} // namespace dag::codec
