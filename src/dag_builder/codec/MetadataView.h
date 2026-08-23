#pragma once

#include <filesystem>
#include <vector>

#include "codec/Dag.h"
#include "codec/DagFormat.h"
#include "io/envelope_file.h"
#include "store/Codec.h"
#include "store/codec/Path.h"

namespace dag::codec {

class MetadataView final : public store::Codec<dag::NodeMetadata> {
public:
    std::vector<std::filesystem::path> paths(const store::NodePath& node_path) const override
    {
        return { store::codec::append_extension(node_path, ".dagmeta") };
    }

    std::expected<dag::NodeMetadata, store::CodecError> read(const store::NodePath& node_path) const override
    {
        auto payload = io::envelope::read_from_path<dag::format::MetadataSchema>(paths(node_path).front());
        if (!payload) {
            return std::unexpected(file_error(store::CodecOperation::Read, payload.error()));
        }
        auto metadata = dag::format::decode_metadata(std::move(*payload));
        if (!metadata) {
            return std::unexpected(store::CodecError {
                store::CodecOperation::Read,
                store::CodecErrorCategory::InvalidData,
                metadata.error(),
            });
        }
        return std::move(*metadata);
    }
};

} // namespace dag::codec
