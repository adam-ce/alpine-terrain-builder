#pragma once

#include <filesystem>
#include <iterator>
#include <optional>
#include <string>

#include "raster_store/StoreTraits.h"
#include "store/path_layout.h"
#include "string_utils.h"

namespace raster_store::path_layout::zoom_xy_google {

inline std::filesystem::path key_to_node_path(const radix::tile::Id& key)
{
    return std::to_string(key.zoom_level) + "/" + std::to_string(key.coords.x) + "/" + std::to_string(key.coords.y);
}

inline std::optional<radix::tile::Id> node_path_to_key(const std::filesystem::path& path)
{
    if (path.is_absolute() || std::distance(path.begin(), path.end()) != 3) {
        return std::nullopt;
    }
    auto part = path.begin();
    const auto zoom = from_chars<std::uint32_t>(part->string());
    ++part;
    const auto x = from_chars<std::uint32_t>(part->string());
    ++part;
    const auto y = from_chars<std::uint32_t>(part->string());
    if (!zoom || !x || !y) {
        return std::nullopt;
    }
    const radix::tile::Id key { *zoom, { *x, *y } };
    return StoreTraits::is_valid(key) ? std::optional(key) : std::nullopt;
}

inline store::path_layout::Mapping<radix::tile::Id> zoom_x_y_google()
{
    return { "zoom/x/y_google", key_to_node_path, node_path_to_key };
}

inline std::optional<store::path_layout::Mapping<radix::tile::Id>> from_id(const std::string_view id)
{
    if (id == zoom_x_y_google().id) {
        return zoom_x_y_google();
    }
    return std::nullopt;
}

} // namespace raster_store::path_layout::zoom_xy_google
