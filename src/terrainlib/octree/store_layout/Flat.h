#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "octree/Id.h"
#include "store/NodePath.h"
#include "string_utils.h"

namespace octree::store_layout {

inline store::NodePath flat_key_to_node_path(const Id &id) {
    return store::NodePath(fmt::format("{}-{}", id.level(), id.index_on_level()));
}

inline std::optional<Id> flat_node_path_to_key(const store::NodePath &node_path) {
    const std::filesystem::path &path = node_path.path();
    if (path.empty() || path.is_absolute() || path.has_parent_path() || path.has_extension()) {
        return std::nullopt;
    }

    const std::string text = path.string();
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
