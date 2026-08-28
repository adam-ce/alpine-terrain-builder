#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <expected>

#include "Error.h"

namespace store {

template <typename NodeData>
class Codec {
public:
    virtual ~Codec() = default;

    virtual std::vector<std::filesystem::path> paths(const std::filesystem::path& node_path) const = 0;

    virtual Expected<NodeData> read(const std::filesystem::path&) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support reading");
    }

    virtual Expected<void> write(const std::filesystem::path&, const NodeData&) const
    {
        return Error::fail(Error::Code::Unsupported, "codec does not support writing");
    }

protected:
    static std::filesystem::path add_extension(std::filesystem::path path, const std::string_view extension)
    {
        path += extension;
        return path;
    }
};

} // namespace store
