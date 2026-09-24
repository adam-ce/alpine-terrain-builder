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
