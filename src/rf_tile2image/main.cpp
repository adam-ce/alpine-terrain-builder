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

#include <cstdlib>
#include <iostream>

#include <CLI/CLI.hpp>

#include "convert.h"

int main(int argc, char** argv)
{
    CLI::App app { "Export one RF .amort tile as a data JPEG and an attribution PNG" };
    rf_tile2image::Options options;
    app.add_option("input", options.input, "Input .amort tile; metadata is discovered in ancestor directories")->required();
    app.add_option("--metadata", options.metadata, "Explicit raster_store.metadata file");
    app.add_option("--output-dir", options.output_directory, "Output directory (default: beside the input)");
    app.add_option("--min", options.minimum, "Scalar value mapped to black (default: finite tile minimum)");
    app.add_option("--max", options.maximum, "Scalar value mapped to white (default: finite tile maximum)");
    app.add_flag("--overwrite", options.overwrite, "Replace existing output images");
    app.footer("Linear scalars use Cubehelix; sRGB RGB8/RGBA8 is exported directly, ignoring alpha.\n"
               "Without range options, constant zero/positive/negative tiles are black/red/blue.\n"
               "Nonfinite pixels are magenta. JPEG quality is 95; PNG attribution colours are lossless.");
    CLI11_PARSE(app, argc, argv);
    auto result = rf_tile2image::convert(options);
    if (!result) {
        std::cerr << result.error().to_string() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << result->data.string() << '\n' << result->attribution.string() << '\n';
    return EXIT_SUCCESS;
}
