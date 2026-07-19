#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <radix/tile.h>
#include <spdlog/spdlog.h>

#include "TileUrlBuilder.h"

namespace cli {

struct Args {
    std::string provider;
    unsigned int zoom;
    unsigned int x;
    unsigned int y;
    TileCoordinateOrder url_coordinate_order;
    TileYDirection url_y_direction;
    unsigned int srs;
    std::filesystem::path output;
    spdlog::level::level_enum log_level;
    bool early_skip;
    std::optional<unsigned int> max_zoom_level;
    std::string layer;
    std::string style;
};

Args parse(int argc, const char *const *argv);

} // namespace cli
