#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <expected>

#include "store/CodecError.h"
#include "store/NodePath.h"

namespace store {

namespace codec {

inline std::filesystem::path append_extension(const NodePath& node_path, const std::string_view extension)
{
    std::filesystem::path result = node_path.path();
    result += extension;
    return result;
}

} // namespace codec

template <typename NodeData>
class Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path> paths(const NodePath& node_path) const = 0;

    virtual std::expected<NodeData, CodecError> read(const NodePath&) const
    {
        return std::unexpected(CodecError::unsupported_operation(CodecOperation::Read, "read"));
    }

    virtual std::expected<void, CodecError> write(const NodePath&, const NodeData&) const
    {
        return std::unexpected(CodecError::unsupported_operation(CodecOperation::Write, "write"));
    }
};

} // namespace store
