#pragma once

#include <filesystem>
#include <string_view>

#include "store/NodePath.h"

namespace store::codec {

inline std::filesystem::path append_extension(const NodePath& node_path, const std::string_view extension)
{
    std::filesystem::path result = node_path.path();
    result += extension;
    return result;
}

} // namespace store::codec
