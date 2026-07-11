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
    app.add_option("--x,--row", args.x, "Root tile x coordinate")->required();
    app.add_option("--y,--col", args.y, "Root tile y coordinate")->required();

    const std::map<std::string, radix::tile::Scheme> scheme_map{
        {"slippymap", radix::tile::Scheme::SlippyMap},
        {"google", radix::tile::Scheme::SlippyMap},
        {"xyz", radix::tile::Scheme::SlippyMap},
        {"tms", radix::tile::Scheme::Tms}};
    args.scheme = radix::tile::Scheme::SlippyMap;
    app.add_option("--scheme", args.scheme, "Tile scheme")
        ->default_val(radix::tile::Scheme::SlippyMap)
        ->transform(CLI::CheckedTransformer(scheme_map, CLI::ignore_case));

    args.srs = 3857;
    app.add_option("--srs", args.srs, "Spatial reference system EPSG code")->default_val(3857);

    args.output = "tiles/{zoom}/{y}/{x}.{ext}";
    app.add_option("--output", args.output, "Output path template")->default_val(args.output);

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
