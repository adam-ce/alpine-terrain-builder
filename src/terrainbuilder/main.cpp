#include <chrono>
#include <filesystem>
#include <vector>
#include <span>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "Dataset.h"
#include "ctb/GlobalMercator.hpp"
#include "ctb/Grid.hpp"
#include "srs.h"

#include "terrainbuilder.h"
#include "log.h"

int run(std::span<char*> args) {
    int argc = args.size();
    char ** argv = args.data();

    CLI::App app{"Terrain Builder"};
    app.allow_windows_style_options();
    argv = app.ensure_utf8(argv);

    std::filesystem::path dataset_path;
    app.add_option("--dataset", dataset_path, "Path to a heightmap dataset file")
        ->required()
        ->check(CLI::ExistingFile);

    std::optional<std::filesystem::path> texture_base_path;
    app.add_option("--textures", texture_base_path, "Path to a folder containing texture tiles in the format of {zoom}/{col}/{row}.jpeg")
        ->check(CLI::ExistingDirectory);

    std::string mesh_srs_input;
    app.add_option("--mesh-srs", mesh_srs_input, "EPSG code of the target srs of the mesh positions")
        ->default_val("EPSG:4978");

    std::string target_srs_input;
    app.add_option("--target-srs", target_srs_input, "EPSG code of the srs of the target bounds or tile id")
        ->default_val("EPSG:3857");

    auto *target = app.add_option_group("target");
    std::vector<double> target_bounds_data;
    target->add_option("--bounds", target_bounds_data, "Target bounds for the reference tile as \"{xmin} {ymin} {width} {height}\"")
        ->expected(4);
    std::vector<unsigned int> target_tile_data;
    target->add_option("--tile", target_tile_data, "Target tile id for the reference tile as \"{zoom} {x} {y}\"")
        ->expected(3)
        ->excludes("--bounds");
    target->require_option(1);

    radix::tile::Scheme target_tile_scheme;
    std::map<std::string, radix::tile::Scheme> scheme_str_map{{"slippymap", radix::tile::Scheme::SlippyMap}, {"google", radix::tile::Scheme::SlippyMap}, {"tms", radix::tile::Scheme::Tms}};
    app.add_option("--scheme", target_tile_scheme, "Target tile id for the reference tile as \"{zoom} {x} {y}\"")
        ->default_val(radix::tile::Scheme::SlippyMap)
        ->excludes("--bounds")
        ->transform(CLI::CheckedTransformer(scheme_str_map, CLI::ignore_case));

    std::filesystem::path output_path;
    app.add_option("--output", output_path, "Path to which the reference mesh will be written to (extension can be .tile, .gltf or .glb)")
        ->required();

    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace;
    const std::map<std::string, spdlog::level::level_enum> log_level_names{
        {"off", spdlog::level::level_enum::off},
        {"critical", spdlog::level::level_enum::critical},
        {"error", spdlog::level::level_enum::err},
        {"warn", spdlog::level::level_enum::warn},
        {"info", spdlog::level::level_enum::info},
        {"debug", spdlog::level::level_enum::debug},
        {"trace", spdlog::level::level_enum::trace}};
    app.add_option("--verbosity", log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case));

    CLI11_PARSE(app, argc, argv);

    Log::init(log_level);

    Dataset dataset(dataset_path);

    OGRSpatialReference tile_srs;
    tile_srs.SetFromUserInput(target_srs_input.c_str());
    tile_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference mesh_srs;
    mesh_srs.SetFromUserInput(mesh_srs_input.c_str());
    mesh_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    OGRSpatialReference texture_srs;
    texture_srs.importFromEPSG(3857);
    texture_srs.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    radix::tile::SrsBounds tile_bounds;
    if (!target_bounds_data.empty()) {
        const glm::dvec2 tile_bounds_min(target_bounds_data[0], target_bounds_data[1]);
        const glm::dvec2 tile_bounds_size(target_bounds_data[2], target_bounds_data[3]);
        tile_bounds = radix::tile::SrsBounds(tile_bounds_min, tile_bounds_min + tile_bounds_size);
    } else {
        const ctb::Grid grid = ctb::GlobalMercator();

        const unsigned int zoom_level = target_tile_data[0];
        const glm::uvec2 tile_coords(target_tile_data[1], target_tile_data[2]);
        const radix::tile::Id target_tile(zoom_level, tile_coords, target_tile_scheme);
        if (!tile_srs.IsSame(&grid.getSRS())) {
            LOG_ERROR("Target tile id is only supported for webmercator reference system");
            exit(1);
        }

        tile_bounds = grid.srsBounds(target_tile, false);
    }

    terrainbuilder::build(
        dataset,
        texture_base_path,
        mesh_srs,
        tile_srs,
        texture_srs,
        tile_bounds,
        output_path);

    return 0;
}

int main(int argc, char **argv) {
    return run(std::span{argv, static_cast<size_t>(argc)});
}
