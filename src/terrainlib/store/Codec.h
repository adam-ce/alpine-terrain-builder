#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <expected>

#include "store/CodecError.h"

namespace store {

template <typename NodeData>
class Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const = 0;

    virtual std::expected<NodeData, CodecError> read(const std::filesystem::path&) const
    {
        return std::unexpected(CodecError::unsupported_operation(CodecOperation::Read, "read"));
    }

    virtual std::expected<void, CodecError> write(const std::filesystem::path&, const NodeData&) const
    {
        return std::unexpected(CodecError::unsupported_operation(CodecOperation::Write, "write"));
    }

protected:
    static std::filesystem::path add_extension(std::filesystem::path path, const std::string_view extension)
    {
        path += extension;
        return path;
    }
};

} // namespace store
