#pragma once
#include "Error.h"
#include <filesystem>
#include <radix/tile.h>
#include <string>

namespace rf_builder::tiles::provider {
enum class YDirection : std::uint8_t { Down, Up };
struct Settings {
    std::string url_pattern;
    YDirection y_direction = YDirection::Down;
    unsigned min_zoom = 0;
    unsigned max_zoom = 0;
    unsigned tile_size = 0;
    bool operator==(const Settings&) const = default;
};
Expected<Settings> parse(const std::string& json);
Expected<Settings> read(const std::filesystem::path& path);
Expected<unsigned> zoom_offset(const Settings& settings, unsigned output_side);
std::string url(const Settings& settings, const radix::tile::Id& key);
} // namespace rf_builder::tiles::provider
