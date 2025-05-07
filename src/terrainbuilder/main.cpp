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

#include "octree/storage.h"
#include "octree/space.h"

radix::geometry::Aabb3d extend_bounds_to_3d(radix::geometry::Aabb2d bounds2d) {
    const double infinity = std::numeric_limits<double>::infinity();
    const glm::dvec3 min(bounds2d.min, -infinity);
    const glm::dvec3 max(bounds2d.max, infinity);
    return radix::geometry::Aabb3d(min, max);
}

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
        ->default_val("EPSG:4978"); // ECEF

    std::string target_srs_input;
    app.add_option("--target-srs", target_srs_input, "EPSG code of the srs of the target bounds or id")
        ->default_val("EPSG:4978"); // ECEF

    auto *target = app.add_option_group("target");
    std::vector<double> target_bounds_data;
    target->add_option("--bounds", target_bounds_data, "Target bounds for the reference mesh as \"{xmin} {width} {ymin} {height} [{zmin} {depth}]\"")
        ->expected(4, 6);
    std::vector<uint32_t> target_tile_data;
    target->add_option("--tile", target_tile_data, "Target tile id for the reference tile as \"{zoom} {x} {y}\"")
        ->expected(3)
        ->excludes("--bounds");
    std::vector<uint64_t> target_node_data;
    target->add_option("--node", target_node_data, "Target node id for the reference node as \"{zoom} {index}\" or \"{zoom} {x} {y} {z}\"")
        ->expected(2, 4)
        ->excludes("--tile")
        ->excludes("--bounds");
    target->require_option(1);

    radix::tile::Scheme target_tile_scheme;
    std::map<std::string, radix::tile::Scheme> scheme_str_map{{"slippymap", radix::tile::Scheme::SlippyMap}, {"google", radix::tile::Scheme::SlippyMap}, {"tms", radix::tile::Scheme::Tms}};
    app.add_option("--scheme", target_tile_scheme, "Target scheme for the tile id")
        ->default_val(radix::tile::Scheme::SlippyMap)
        ->needs("--tile")
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

    OGRSpatialReference target_bounds_srs = srs::from_user_input(target_srs_input);
    OGRSpatialReference mesh_srs = srs::from_user_input(mesh_srs_input);
    OGRSpatialReference texture_srs = srs::webmercator();

    radix::geometry::Aabb3d target_bounds;
    if (!target_bounds_data.empty()) {
        // Target bounds are given directly
        if (target_bounds_data.size() == 4) {
            const glm::dvec2 min(target_bounds_data[0], target_bounds_data[2]);
            const glm::dvec2 max(target_bounds_data[0] + target_bounds_data[1], target_bounds_data[2] + target_bounds_data[3]);
            target_bounds = extend_bounds_to_3d(radix::geometry::Aabb2d(min, max));
        } else if (target_bounds_data.size() == 6) {
            const glm::dvec3 min(target_bounds_data[0], target_bounds_data[2], target_bounds_data[4]);
            const glm::dvec3 max(target_bounds_data[0] + target_bounds_data[1], target_bounds_data[2] + target_bounds_data[3], target_bounds_data[4] + target_bounds_data[5]);
            target_bounds = radix::geometry::Aabb3d(min, max);
        } else {
            LOG_ERROR("Invalid number of elements for --bounds. Expected 4 or 6 ({xmin} {width} {ymin} {height} [{zmin} {depth}]).");
            return 1;
        }
    } else if (!target_tile_data.empty()) {
        // Target bounds are based on a webmercator tile id
        const ctb::Grid grid = ctb::GlobalMercator();

        const unsigned int zoom_level = target_tile_data[0];
        const glm::uvec2 tile_coords(target_tile_data[1], target_tile_data[2]);
        const radix::tile::Id target_tile(zoom_level, tile_coords, target_tile_scheme);
        if (!target_bounds_srs.IsSame(&grid.getSRS())) {
            LOG_ERROR("Target tile id is only supported for the webmercator reference system");
            exit(1);
        }

        target_bounds = extend_bounds_to_3d(grid.srsBounds(target_tile, false));
    } else {
        // Target bounds are based on an octree node id
        const uint32_t zoom_level = target_node_data[0];
        octree::Id target_node = octree::Id::root();
        if (target_node_data.size() == 2) {
            const uint64_t node_index = target_node_data[1];
            target_node = {zoom_level, node_index};
        } else if (target_node_data.size() == 4) {
            const glm::uvec3 coords(target_node_data[1], target_node_data[2], target_node_data[3]);
            target_node = { zoom_level, coords };
        } else {
            LOG_ERROR("Invalid number of args for --node. Expected 2 or 4.");
            return 1;
        }

        const octree::Space world = octree::Space::earth();
        target_bounds = world.get_node_bounds(target_node);
    }


    terrainbuilder::build(
        dataset,
        target_bounds_srs,
        target_bounds,
        texture_srs,
        texture_base_path,
        mesh_srs,
        output_path);

    return 0;
}

int main(int argc, char **argv) {
    return run(std::span{argv, static_cast<size_t>(argc)});
}
