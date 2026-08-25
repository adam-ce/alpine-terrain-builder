#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "octree/Id.h"
#include "string_utils.h"

namespace octree::store_layout {

inline std::filesystem::path flat_key_to_node_path(const Id& id) { return fmt::format("{}-{}", id.level(), id.index_on_level()); }

inline std::optional<Id> flat_node_path_to_key(const std::filesystem::path& node_path)
{
    if (node_path.empty() || node_path.is_absolute() || node_path.has_parent_path() || node_path.has_extension()) {
        return std::nullopt;
    }

    const std::string text = node_path.string();
    const size_t separator = text.find('-');
    if (separator == std::string::npos || separator != text.rfind('-')) {
        return std::nullopt;
    }
    const auto level = from_chars<Id::Level>(std::string_view(text).substr(0, separator));
    const auto index = from_chars<Id::Index>(std::string_view(text).substr(separator + 1));
    if (!level.has_value() || !index.has_value()) {
        return std::nullopt;
    }
    return Id::try_make(level.value(), index.value());
}

} // namespace octree::store_layout
