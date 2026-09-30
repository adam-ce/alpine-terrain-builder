/*****************************************************************************
 * AlpineMaps.org
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

#include "Error.h"
#include "io/image.h"

namespace rf_tile2image {

struct Options {
    std::filesystem::path input;
    std::filesystem::path metadata;
    std::filesystem::path output_directory;
    std::optional<long double> minimum = std::nullopt;
    std::optional<long double> maximum = std::nullopt;
    bool overwrite = false;
};

struct Images {
    // RGB byte images, in the stored raster's north-to-south row order.
    io::image::RGB8 data;
    io::image::RGB8 attribution;
};

struct OutputPaths {
    std::filesystem::path data;
    std::filesystem::path attribution;
};

Expected<Images> render_tile(const Options& options);
Expected<OutputPaths> convert(const Options& options);

} // namespace rf_tile2image
