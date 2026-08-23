#pragma once

#include <filesystem>
#include <vector>

#include <expected>

#include "store/CodecError.h"
#include "store/NodePath.h"

namespace store {

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
