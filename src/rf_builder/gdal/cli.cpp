#include "cli.h"
namespace rf_builder::gdal::cli {
void configure(CLI::App& app, Options& options)
{
    app.add_option("--dataset", options.dataset, "Prepared raster or VRT path/URL")->required();
    app.add_option("--mask", options.mask, "Vector validity mask (split polygons at the antimeridian)")->required();
    app.add_option("--output", options.output, "New final snapshot path; log appends to <output>.log")->required();
    app.add_option("--attribution-index", options.attribution_index, "Existing attribution entry (1..65534)")->required();
    app.add_option_function<std::string>(
           "--mode", [&options](const auto& value) { options.mode = value == "rgb" ? Mode::Colour : Mode::Scalar; }, "Output representation")
        ->check(CLI::IsMember({ "scalar", "rgb" }))
        ->default_val("scalar");
    app.add_option("--bands", options.bands, "One scalar band or three bands in RGB order")->expected(1, 3);
    app.add_option("--tile-size", options.tile_side, "Pixels per side")->check(CLI::PositiveNumber)->default_val(4096);
    app.add_option("--jobs", options.jobs, "Concurrent tile workers")->check(CLI::PositiveNumber)->default_val(1);
    app.add_option_function<std::string>("--cache", [&options](const auto& path) { options.cache = path; }, "Compatible incomplete .part snapshot to reuse");
}
} // namespace rf_builder::gdal::cli
