#include <algorithm>
#include <execution>
#include <filesystem>
#include <span>
#include <vector>
#include <cstdint>

#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>
#include <tbb/global_control.h>

#include "Dataset.h"
#include "log.h"
#include "srs.h"
#include "terrainbuilder.h"
#include "octree/Space.h"
#include "tile_provider.h"

#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"
#include "ctb/Grid.hpp"

OGRSpatialReference parse_srs(const std::string &user_input) {
    const auto result = srs::from_user_input(user_input);
    if (!result.has_value()) {
        LOG_ERROR(result.error());
        exit(1);
    }
    return result.value();
}

int32_t srs_dimension(const OGRSpatialReference &srs) {
    if (srs.IsCompound())
        return 3;
    if (srs.IsGeographic() || srs.IsProjected())
        return 2;
    return 2; // fallback
}

radix::geometry::Aabb3d extend_bounds_to_3d(radix::geometry::Aabb2d bounds2d) {
    const double infinity = std::numeric_limits<double>::infinity();
    const glm::dvec3 min(bounds2d.min, -infinity);
    const glm::dvec3 max(bounds2d.max, infinity);
    return radix::geometry::Aabb3d(min, max);
}

radix::geometry::Aabb3d parse_bounds_from_values(const std::vector<double> &data, const OGRSpatialReference &srs) {
    const int32_t dim = srs_dimension(srs);
    const size_t expected = (dim == 3) ? 6 : 4;

    if (data.size() != expected) {
        LOG_ERROR_AND_EXIT("Target bounds expects {} values for {}D SRS.", expected, dim);
    }

    if (data.size() == 4) {
        const glm::dvec2 min(data[0], data[2]);
        const glm::dvec2 max(data[0] + data[1], data[2] + data[3]);
        return extend_bounds_to_3d(radix::geometry::Aabb2d(min, max));
    } else if (data.size() == 6) {
        const glm::dvec3 min(data[0], data[2], data[4]);
        const glm::dvec3 max(data[0] + data[1], data[2] + data[3], data[4] + data[5]);
        return radix::geometry::Aabb3d(min, max);
    } else {
        LOG_ERROR_AND_EXIT("Invalid number of elements for --bounds. Expected 4 or 6.");
    }
}

radix::geometry::Aabb3d parse_bounds_from_tile(
    const std::vector<uint32_t> &data,
    const OGRSpatialReference &srs) {

    // Determine the correct Grid type based on SRS
    std::optional<ctb::Grid> grid;
    const OGRSpatialReference &webmercator_srs = srs::webmercator();
    const OGRSpatialReference &wgs84_srs = srs::wgs84();
    if (srs.IsSame(&webmercator_srs)) {
        grid = ctb::GlobalMercator();
    } else if (srs.IsSame(&wgs84_srs)) {
        grid = ctb::GlobalGeodetic();
    } else {
        LOG_ERROR_AND_EXIT("Only WebMercator (EPSG:3857) or WGS84 (EPSG:4326) supported for --tile.");
    }

    const uint32_t zoom_level = data[0];
    const glm::uvec2 tile_coords(data[1], data[2]);
    const radix::tile::Id target_tile(zoom_level, tile_coords);

    return extend_bounds_to_3d(grid->srsBounds(target_tile, false));
}

radix::geometry::Aabb3d parse_bounds_from_node(const std::vector<uint64_t> &data, const OGRSpatialReference &srs) {
    // Validate node SRS (only ECEF)
    const OGRSpatialReference &ecef_srs = srs::ecef();
    if (!srs.IsSame(&ecef_srs)) {
        LOG_ERROR_AND_EXIT("Only ECEF (EPSG:4978) supported for --node.");
    }

    const octree::Id::Level zoom_level = data[0];
    octree::Id target_node = octree::Id::root();

    if (data.size() == 2) {
        target_node = {zoom_level, data[1]};
    } else if (data.size() == 4) {
        const octree::Id::Coords coords(data[1], data[2], data[3]);
        target_node = {zoom_level, coords};
    } else {
        LOG_ERROR_AND_EXIT("Invalid number of args for --node. Expected 2 or 4.");
    }

    return octree::Space::earth().get_node_bounds(target_node);
}

radix::geometry::Aabb3d parse_target_bounds(
    const std::vector<double> &bounds_data,
    const std::vector<uint64_t> &node_data,
    const std::vector<uint32_t> &tile_data,
    OGRSpatialReference &srs) {

    if (!bounds_data.empty()) {
        return parse_bounds_from_values(bounds_data, srs);
    }

    if (!tile_data.empty()) {
        return parse_bounds_from_tile(tile_data, srs);
    }

    if (!node_data.empty()) {
        return parse_bounds_from_node(node_data, srs);
    }

    UNREACHABLE();
}

void log_dataset_overview(const Dataset &dataset) {
    const std::string name = dataset.name();

    const uint32_t widthPx = dataset.widthInPixels();
    const uint32_t heightPx = dataset.heightInPixels();
    const uint32_t bands = dataset.n_bands();

    const auto datasetSrs = dataset.srs();
    const auto bounds = dataset.bounds3d(true);

    const auto boundsEcef =
        srs::encompassing_bounds_transfer(datasetSrs, srs::ecef(), bounds);

    const octree::Space earth = octree::Space::earth();
    const auto enclosingNode =
        earth.find_smallest_node_encompassing_bounds(boundsEcef).value();

    const int32_t scaleLevel = static_cast<int32_t>(std::floor(std::log2(
        earth.bounds().size().x / glm::compMax(boundsEcef.size()))));

    LOG_INFO("Dataset");
    LOG_INFO("  name        : {}", name);
    LOG_INFO("  size        : {} x {} px | bands={}", widthPx, heightPx, bands);
    LOG_INFO("  srs         : {}", srs::friendly_name(datasetSrs));
    LOG_INFO("  bounds      : [{:.3f}, {:.3f}, {:.3f}] - [{:.3f}, {:.3f}, {:.3f}]",
             bounds.min.x, bounds.min.y, bounds.min.z,
             bounds.max.x, bounds.max.y, bounds.max.z);
    LOG_INFO("  bounds ecef : [{:.3f}, {:.3f}, {:.3f}] - [{:.3f}, {:.3f}, {:.3f}]",
             boundsEcef.min.x, boundsEcef.min.y, boundsEcef.min.z,
             boundsEcef.max.x, boundsEcef.max.y, boundsEcef.max.z);
    LOG_INFO("  octree      : enclosing node level={} coords=({}, {}, {})",
             enclosingNode.level(),
             enclosingNode.coords().x,
             enclosingNode.coords().y,
             enclosingNode.coords().z);
    LOG_INFO("  octree      : scale level={}", scaleLevel);
}

int run(std::span<char *> args) {
    int argc = args.size();
    char **argv = args.data();

    CLI::App app{"sf_builder"};
    app.allow_windows_style_options();
    app.require_subcommand(1, 1);
    argv = app.ensure_utf8(argv);

    // === COMMON OPTIONS ===
    std::filesystem::path dataset_path;
    std::optional<std::filesystem::path> texture_base_path;
    std::string mesh_srs_input = "EPSG:4978";
    std::optional<uint32_t> min_texture_level;
    std::optional<uint32_t> max_texture_level;
    std::filesystem::path output_path;
    spdlog::level::level_enum log_level = spdlog::level::level_enum::trace;

    const std::map<std::string, spdlog::level::level_enum> log_level_names{
        {"off", spdlog::level::level_enum::off},
        {"critical", spdlog::level::level_enum::critical},
        {"error", spdlog::level::level_enum::err},
        {"warn", spdlog::level::level_enum::warn},
        {"info", spdlog::level::level_enum::info},
        {"debug", spdlog::level::level_enum::debug},
        {"trace", spdlog::level::level_enum::trace}};

    app.add_option("--dataset", dataset_path, "Path to a heightmap dataset file")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--textures", texture_base_path, "Path to a folder containing texture tiles in the format of {zoom}/{col}/{row}.jpeg")
        ->check(CLI::ExistingDirectory);
    app.add_option("--mesh-srs", mesh_srs_input, "EPSG code of the target srs of the mesh positions")
        ->default_val("EPSG:4978");
    app.add_option("--min-texture-level", min_texture_level, "Minimum texture zoom level to use for assembling textures.")
        ->needs("--textures");
    app.add_option("--max-texture-level", max_texture_level, "Maximum texture zoom level to use for assembling textures.")
        ->needs("--textures");
    app.add_option("--verbosity", log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case));

    // === SINGLE COMMAND ===
    auto *single = app.add_subcommand("single", "Build a single reference mesh")
                       ->fallthrough();

    std::vector<double> target_bounds_data;
    std::vector<uint32_t> target_tile_data;
    std::vector<uint64_t> target_node_data;
    std::string target_srs_input;

    auto *target = single->add_option_group("target");
    target->add_option("--bounds", target_bounds_data, "Target bounds for the reference mesh as \"{xmin} {width} {ymin} {height} [{zmin} {depth}]\"")
        ->expected(4, 6);
    target->add_option("--tile", target_tile_data, "Target tile id for the reference tile as \"{zoom} {x} {y}\"")
        ->expected(3);
    target->add_option("--node", target_node_data, "Target node id for the reference node as \"{zoom} {index}\" or \"{zoom} {x} {y} {z}\"")
        ->expected(2, 4);
    target->require_option(1);

    single->add_option("--output", output_path, "Output path were the mesh is written to (.sfmesh, .gltf or .glb)")
        ->required();

    single->add_option("--srs", target_srs_input, "EPSG code of the srs of the target bounds or id");
    single->callback([&]() {
        if (target_srs_input.empty()) {
            if (!target_bounds_data.empty()) {
                target_srs_input = "EPSG:4978";
            } else if (!target_tile_data.empty()) {
                target_srs_input = "EPSG:3857";
            } else if (!target_node_data.empty()) {
                target_srs_input = "EPSG:4978";
            }
        }
    });

    // === BATCH COMMAND ===
    auto *batch = app.add_subcommand("batch", "Build all nodes for a dataset at a given level")
                      ->fallthrough();

    octree::Id::Level target_level;
    batch->add_option("--target-level", target_level, "Level of detail for batch generation")
        ->required();
    std::filesystem::path output_base_path;
    batch->add_option("--output", output_base_path, "Output path were the meshes are written to.")
        ->required();
    std::string output_format;
    batch->add_option("--format", output_format, "Output mesh format")
        ->check(CLI::IsMember({".glb", ".gltf", ".sfmesh"}))
        ->default_val(".sfmesh");
    uint32_t num_threads = 0;
    batch->add_option("--threads", num_threads, "Number of threads to use")
        ->check(CLI::PositiveNumber);
    bool overwrite_existing = false;
    batch->add_flag("--overwrite", overwrite_existing, "Overwrite existing mesh files");

    CLI11_PARSE(app, argc, argv);

    Log::init(log_level);

    Dataset dataset(dataset_path.string());
    log_dataset_overview(dataset);
    OGRSpatialReference mesh_srs = parse_srs(mesh_srs_input);
    OGRSpatialReference texture_srs = srs::webmercator();

    std::unique_ptr<TileProvider> tile_provider;
    if (texture_base_path.has_value()) {
        GoogleMapboxTilePathProvider basemap_provider(texture_base_path.value());
        if (min_texture_level.has_value() || max_texture_level.has_value()) {
            tile_provider = std::make_unique<ZoomRangeTileProvider<GoogleMapboxTilePathProvider>>(
                std::move(basemap_provider),
                min_texture_level,
                max_texture_level);
        } else {
            tile_provider = std::make_unique<GoogleMapboxTilePathProvider>(std::move(basemap_provider));
        }
    }

    if (*single) {
        OGRSpatialReference target_srs = parse_srs(target_srs_input);
        const radix::geometry::Aabb3d target_bounds = parse_target_bounds(
            target_bounds_data,
            target_node_data,
            target_tile_data,
            target_srs);

        terrainbuilder::build_and_save_patch(
            dataset,
            target_srs,
            target_bounds,
            texture_srs,
            tile_provider.get(),
            mesh_srs,
            output_path);
    }

    if (*batch) {
        std::optional<tbb::global_control> tbb_control;
        if (num_threads > 0) {
            LOG_DEBUG("Requesting max parallelism of {}", num_threads);
            tbb_control.emplace(tbb::global_control::max_allowed_parallelism, num_threads);
        }
        const auto actual_num_threads = tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism);
        const auto maybe_s = actual_num_threads == 1 ? "" : "s";
        LOG_INFO("Using {} thread{} for batch processing.", actual_num_threads, maybe_s);

        const auto build_result = terrainbuilder::build_all_patches(
            dataset,
            target_level,
            texture_srs,
            tile_provider.get(),
            mesh_srs,
            output_base_path,
            output_format,
            overwrite_existing);
        if (!build_result.has_value()) {
            LOG_ERROR(
                "Failed to finalize Structura Fundamentalis output: {}",
                build_result.error().to_string());
            return 1;
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    return run(std::span{argv, static_cast<size_t>(argc)});
}
