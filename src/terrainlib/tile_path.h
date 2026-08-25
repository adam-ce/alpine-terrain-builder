#pragma once

#include <filesystem>
#include <string>

#include <radix/tile.h>

[[nodiscard]] inline std::filesystem::path google_tile_path(
    const std::filesystem::path& base_path,
    const radix::tile::Id& tile_id,
    const std::string& extension)
{
    return base_path / std::to_string(tile_id.zoom_level) / std::to_string(tile_id.coords.x) / (std::to_string(tile_id.coords.y) + extension);
}
