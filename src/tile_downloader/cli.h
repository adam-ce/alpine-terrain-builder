/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
