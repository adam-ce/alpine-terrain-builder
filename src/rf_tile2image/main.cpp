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
