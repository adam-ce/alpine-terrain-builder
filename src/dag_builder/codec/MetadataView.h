#pragma once

#include <filesystem>
#include <vector>

#include <fmt/core.h>

#include "io/serialize.h"
#include "serialization.h"
#include "store/Codec.h"
#include "store/codec/ZppBits.h"

namespace dag::codec {

class MetadataView final : public store::Codec<dag::NodeMetadata> {
public:
    std::vector<std::filesystem::path> paths(
        const store::NodePath &node_path) const override {
        return {store::codec::append_extension(node_path, ".bin")};
    }

    std::expected<dag::NodeMetadata, store::CodecError> read(
        const store::NodePath &node_path) const override {
        const auto result = io::read_from_path<dag::NodeMetadata>(paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(store::CodecError{
                store::CodecOperation::Read,
                result.error() == io::Error(io::Error::OpenFile)
                    ? store::CodecErrorCategory::FileNotFound
                    : store::CodecErrorCategory::Io,
                fmt::format("{}", result.error()),
            });
        }
        return result.value();
    }
};

} // namespace dag::codec
