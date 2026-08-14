#pragma once

#include <filesystem>
#include <vector>

#include <fmt/format.h>

#include "io/serialize.h"
#include "store/Codec.h"

namespace store::codec {

inline std::filesystem::path append_extension(
    const NodePath &node_path,
    const std::string_view extension) {
    std::filesystem::path result = node_path.path();
    result += extension;
    return result;
}

template<typename NodeData>
class ZppBits final : public Codec<NodeData> {
public:
    std::vector<std::filesystem::path> paths(const NodePath &node_path) const override {
        return {append_extension(node_path, ".bin")};
    }

    std::expected<NodeData, CodecError> read(const NodePath &node_path) const override {
        const auto result = io::read_from_path<NodeData>(paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(CodecError{
                CodecOperation::Read,
                result.error() == io::Error(io::Error::OpenFile)
                    ? CodecErrorCategory::FileNotFound
                    : CodecErrorCategory::Io,
                fmt::format("{}", result.error()),
            });
        }
        return result.value();
    }

    std::expected<void, CodecError> write(
        const NodePath &node_path,
        const NodeData &data) const override {
        const auto result = io::write_to_path(data, paths(node_path).front());
        if (!result.has_value()) {
            return std::unexpected(CodecError{
                CodecOperation::Write,
                CodecErrorCategory::Io,
                fmt::format("{}", result.error()),
            });
        }
        return {};
    }
};

} // namespace store::codec
