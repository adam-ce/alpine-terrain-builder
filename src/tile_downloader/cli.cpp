#include "cli.h"

#include <map>
#include <string>

#include <CLI/CLI.hpp>

namespace cli {

Args parse(int argc, const char *const *argv) {
    CLI::App app{"tile-downloader"};
    app.allow_windows_style_options();

    Args args;

    const std::map<std::string, TileDownloadProvider> provider_map{
        {"basemap", TileDownloadProvider::Basemap},
        {"gataki", TileDownloadProvider::Gataki}};
    auto* source_group = app.add_option_group("Tile source");
    source_group->add_option("--provider", args.provider, "Configured tile provider (basemap or gataki)")
        ->transform(CLI::CheckedTransformer(provider_map, CLI::ignore_case));
    auto* url_option = source_group->add_option("--url", args.url_pattern, "Custom tile URL pattern containing {zoom}, {x}, and {y}");
    source_group->require_option(1);

    app.add_option("--zoom", args.zoom, "Root tile zoom level")->required();
    app.add_option("--x,--col", args.x, "Root tile x/column in Google/Mapbox coordinates")->required();
    app.add_option("--y,--row", args.y, "Root tile y/row in Google/Mapbox coordinates")->required();

    const std::map<std::string, TileYDirection> y_direction_map{
        {"down", TileYDirection::Down},
        {"up", TileYDirection::Up}};
    args.url_y_direction = TileYDirection::Down;
    app.add_option("--url-y-direction", args.url_y_direction, "Custom URL y direction: down (Google/Mapbox) or up (legacy TMS)")
        ->default_str("down")
        ->needs(url_option)
        ->transform(CLI::CheckedTransformer(y_direction_map, CLI::ignore_case));

    args.srs = 3857;
    app.add_option("--srs", args.srs, "Spatial reference system EPSG code")->default_val(3857);

    args.output = "tiles";
    app.add_option("--output", args.output, "Output directory; files use Google/Mapbox zoom/x/y.jpeg layout")->default_val(args.output.string());

    const std::map<std::string, spdlog::level::level_enum> log_level_names{
        {"off", spdlog::level::off},
        {"critical", spdlog::level::critical},
        {"error", spdlog::level::err},
        {"warn", spdlog::level::warn},
        {"info", spdlog::level::info},
        {"debug", spdlog::level::debug},
        {"trace", spdlog::level::trace}};
    args.log_level = spdlog::level::info;
    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val(spdlog::level::info);

    args.early_skip = true;
    app.add_option("--early-skip", args.early_skip, "Resume optimization: skip completed subtrees")
        ->default_val(true);

    app.add_option("--max-zoom-level", args.max_zoom_level, "Maximum zoom level to descend to");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }

    return args;
}

} // namespace cli
