#include "cli.h"

#include <map>
#include <string>

#include <CLI/CLI.hpp>

namespace cli {

Args parse(int argc, const char *const *argv) {
    CLI::App app{"tile-downloader"};
    app.allow_windows_style_options();

    Args args;

    app.add_option("--provider", args.provider, "Tile provider (basemap or gataki)")
        ->required()
        ->check(CLI::IsMember({"basemap", "gataki"}, CLI::ignore_case));

    app.add_option("--zoom", args.zoom, "Root tile zoom level")->required();
    app.add_option("--x,--col", args.x, "Root tile x/column in Google/Mapbox coordinates")->required();
    app.add_option("--y,--row", args.y, "Root tile y/row in Google/Mapbox coordinates")->required();

    const std::map<std::string, TileCoordinateOrder> coordinate_order_map{
        {"xy", TileCoordinateOrder::Xy},
        {"yx", TileCoordinateOrder::Yx}};
    args.url_coordinate_order = TileCoordinateOrder::Xy;
    app.add_option("--url-coordinate-order", args.url_coordinate_order, "URL coordinate order: xy (common Google/Mapbox format) or yx")
        ->default_str("xy")
        ->transform(CLI::CheckedTransformer(coordinate_order_map, CLI::ignore_case));

    const std::map<std::string, TileYDirection> y_direction_map{
        {"down", TileYDirection::Down},
        {"up", TileYDirection::Up}};
    args.url_y_direction = TileYDirection::Down;
    app.add_option("--url-y-direction", args.url_y_direction, "URL y direction: down (common Google/Mapbox format) or up (legacy TMS)")
        ->default_str("down")
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

    args.layer = "bmaporthofoto30cm";
    app.add_option("--layer", args.layer, "Basemap layer name")->default_val(args.layer);

    args.style = "normal";
    app.add_option("--style", args.style, "Basemap style")->default_val(args.style);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }

    return args;
}

} // namespace cli
