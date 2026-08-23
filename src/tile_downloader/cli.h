#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <spdlog/spdlog.h>

#include "TileUrlBuilder.h"

namespace cli {

struct Args {
    std::optional<TileDownloadProvider> provider = std::nullopt;
    std::optional<std::string> url_pattern = std::nullopt;
    unsigned int zoom;
    unsigned int x;
    unsigned int y;
    TileYDirection url_y_direction;
    unsigned int srs;
    std::filesystem::path output;
    spdlog::level::level_enum log_level;
    std::optional<unsigned int> max_zoom_level;
};

Args parse(int argc, const char *const *argv);

} // namespace cli
