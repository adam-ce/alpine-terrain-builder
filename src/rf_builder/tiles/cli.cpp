#include "cli.h"
namespace rf_builder::tiles::cli {
void configure(CLI::App& app, Options& options)
{
    app.add_option("--provider", options.provider, "Provider JSON file with all source settings")->required();
    app.add_option("--mask", options.mask, "Vector validity mask (split polygons at the antimeridian)")->required();
    app.add_option("--output", options.output.output, "New final snapshot path; log appends to <output>.log")->required();
    app.add_option("--attribution-index", options.output.attribution_index, "Existing attribution entry (1..65534)")->required();
    app.add_option("--tile-size", options.output.tile_side, "RF pixels per side: source tile size times a power of two")
        ->check(CLI::PositiveNumber)
        ->default_val(4096);
    app.add_option("--jobs", options.output.jobs, "Concurrent tile workers and maximum simultaneous requests")->check(CLI::PositiveNumber)->default_val(1);
    app.add_option_function<std::string>(
           "--value-mapping",
           [&options](const auto& value) {
               options.output.value_mapping = value == "linear" ? raster_store::pixel::Mapping::Linear : raster_store::pixel::Mapping::SRGBA;
           },
           "Stored value mapping override: RGB/RGBA uint8 defaults to srgba (linear alpha); all other pixels default to linear")
        ->check(CLI::IsMember({ "linear", "srgba" }));
    app.add_option_function<std::string>(
        "--cache", [&options](const auto& path) { options.output.cache = path; }, "Compatible incomplete .part snapshot to reuse");
    app.footer(R"(Example:
  rf-builder tiles --provider providers/basemap.json --mask validity.gpkg --output new-rf --attribution-index 1

Provider JSON (illustrative; all fields required):
  {"url_pattern":"https://example.org/{zoom}/{x}/{y}.jpeg","y_direction":"down","min_zoom":4,"max_zoom":20,"tile_size":256}

Source max_zoom is a ceiling: retain each region's deepest available imagery.
Source min_zoom must be at least log2(RF tile size / source tile size).
Only HTTP 404 means absence. Transient errors retry with waits starting at
500 ms, doubling within a one-hour total request/retry deadline.
)");
}
} // namespace rf_builder::tiles::cli
