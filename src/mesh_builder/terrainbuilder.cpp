#include <chrono>
#include <vector>

#include <fmt/core.h>
#include <glm/glm.hpp>
#include <radix/geometry.h>

#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for.h>
#include <tbb/task_group.h>

#include "Dataset.h"
#include "ctb/GlobalMercator.hpp"
#include "ctb/Grid.hpp"
#include "srs.h"

#include "ProgressIndicator.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "mesh_builder.h"
#include "terrainbuilder.h"
#include "texture_assembler.h"
#include "tile_provider.h"
#include "mesh/validate.h"

#include "octree/Id.h"
#include "octree/Space.h"
#include "octree/Storage.h"
#include "octree/disk/layout/strategy/LevelAndCoordinateDirectories.h"
#include "octree/utils.h"

namespace terrainbuilder {

namespace {
std::string format_secs_since(const std::chrono::high_resolution_clock::time_point &start) {
    const auto duration = std::chrono::high_resolution_clock::now() - start;
    const double seconds = std::chrono::duration<double>(duration).count();
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << seconds;
    return ss.str();
}
}

class BasemapSchemeTilePathProvider : public TilePathProvider {
public:
    BasemapSchemeTilePathProvider(std::filesystem::path base_path)
        : base_path(base_path) {}

    std::optional<std::filesystem::path> get_tile_path(const radix::tile::Id tile_id) const override {
        return fmt::format("{}/{}/{}/{}.jpeg", this->base_path.string(), tile_id.zoom_level, tile_id.coords.y, tile_id.coords.x);
    }

private:
    std::filesystem::path base_path;
};

std::optional<SimpleMesh> build_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const std::optional<std::filesystem::path> texture_base_path,
    const OGRSpatialReference &mesh_srs) {
    const ctb::Grid grid = ctb::GlobalMercator();
    radix::tile::SrsBounds texture_bounds;

    std::chrono::high_resolution_clock::time_point start;
    start = std::chrono::high_resolution_clock::now();
    LOG_INFO("Building mesh...");
    tl::expected<SimpleMesh, BuildMeshError> mesh_result = build_reference_mesh_patch(
        dataset,
        mesh_srs,
        target_bounds_srs, target_bounds,
        texture_srs, texture_bounds);
    if (!mesh_result.has_value()) {
        const BuildMeshError error = mesh_result.error();
        if (error == BuildMeshError::OutOfBounds) {
            const radix::tile::SrsBounds dataset_bounds = dataset.bounds();
            LOG_ERROR("Target bounds are fully outside of dataset region\n"
                      "Dataset {{\n"
                      "\t x={}, y={}, w={}, h={}.\n"
                      "}}\n"
                      "Target {{\n"
                      "\t x={}, y={}, w={}, h={}.\n"
                      "}}",
                      dataset_bounds.min.x, dataset_bounds.min.y, dataset_bounds.width(), dataset_bounds.height(),
                      target_bounds.min.x, target_bounds.min.y, target_bounds.size().x, target_bounds.size().y);
            return std::nullopt;
        } else if (error == BuildMeshError::EmptyRegion) {
            LOG_WARN("Target bounds are inside dataset, but the region is empty");
            return std::nullopt;
        }
    }
    SimpleMesh mesh = mesh_result.value();
    LOG_DEBUG("Mesh building took {}s", format_secs_since(start));
    LOG_INFO("Finished building mesh geometry");

    if (texture_base_path.has_value()) {
        start = std::chrono::high_resolution_clock::now();
        LOG_INFO("Assembling mesh texture");
        BasemapSchemeTilePathProvider tile_provider(texture_base_path.value());
        std::optional<cv::Mat> texture = assemble_texture_from_tiles(grid, texture_srs, texture_bounds, tile_provider);
        if (!texture.has_value()) {
            LOG_ERROR("Failed to assemble texture");
            // TODO: should we return nullopt here?
        }
        mesh.texture = texture;
        LOG_DEBUG("Assembling mesh texture took {}s", format_secs_since(start));
        LOG_INFO("Finished assembling mesh texture");
    } else {
        LOG_INFO("Skipped assembling texture");
    }
    
    return mesh;
}

void build_and_save_patch(
    Dataset &dataset,
    const OGRSpatialReference &target_bounds_srs,
    const radix::geometry::Aabb3d &target_bounds,
    const OGRSpatialReference &texture_srs,
    const std::optional<std::filesystem::path> texture_base_path,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_path) {
    auto mesh_result = build_patch(
        dataset,
        target_bounds_srs,
        target_bounds,
        texture_srs,
        texture_base_path,
        mesh_srs);
    if (!mesh_result.has_value()) {
        return;
    }
    const SimpleMesh mesh = std::move(mesh_result.value());

    LOG_INFO("Writing mesh to output path {}", output_path);

    std::chrono::high_resolution_clock::time_point start;
    start = std::chrono::high_resolution_clock::now();
    // TODO: use a JSON libary instead
    std::unordered_map<std::string, std::string> metadata;
    metadata["mesh_srs"] = mesh_srs.GetAuthorityCode(nullptr);
    metadata["bounds_srs"] = target_bounds_srs.GetAuthorityCode(nullptr);
    metadata["texture_srs"] = texture_srs.GetAuthorityCode(nullptr);
    metadata["bounds"] = fmt::format(
        "{{ \"min\": {{ \"x\": {}, \"y\": {} }}, \"max\": {{ \"x\": {}, \"y\": {} }} }}",
        target_bounds.min.x, target_bounds.min.y, target_bounds.max.x, target_bounds.max.y);
    // metadata["texture_bounds"] = fmt::format(
    //     "{{ \"min\": {{ \"x\": {}, \"y\": {} }}, \"max\": {{ \"x\": {}, \"y\": {} }} }}",
    //    texture_bounds.min.x, texture_bounds.min.y, texture_bounds.max.x, texture_bounds.max.y);
    if (!mesh::io::save_to_path(mesh, output_path, mesh::io::SaveOptions{.metadata = metadata}).has_value()) {
        LOG_ERROR("Failed to save mesh to file {}", output_path);
        exit(2);
    }
    LOG_DEBUG("Writing mesh took {}s", format_secs_since(start));
    LOG_INFO("Done", output_path);
}

namespace {
template <typename T>
T expect(const std::optional<T> &opt, const std::string &msg) {
    if (!opt) {
        LOG_ERROR_AND_EXIT(msg);
    }
    return *opt;
}
}

void build_all_patches(
    Dataset &dataset,
    const octree::Id::Level target_level,
    const OGRSpatialReference &texture_srs,
    const std::optional<std::filesystem::path> &texture_base_path,
    const OGRSpatialReference &mesh_srs,
    const std::filesystem::path &output_base_path,
    const std::string &output_format,
    const bool overwrite_existing
) {
    if (!std::filesystem::exists(output_base_path)) {
        LOG_TRACE("Output base path {} does not exist, creating it", output_base_path);
        std::filesystem::create_directories(output_base_path);
    } else if (!std::filesystem::is_directory(output_base_path)) {
        LOG_ERROR_AND_EXIT("Output base path {} exists but is not a directory", output_base_path);
    }

    octree::Storage storage = octree::open_folder(
        output_base_path,
        false,
        octree::OpenOptions {
            .preferred_extension_with_dot = output_format
        }
    );

    const auto dataset_srs = dataset.srs();
    const auto dataset_bounds = dataset.bounds3d(true);

    const auto ecef_srs = srs::ecef();
    const auto ecef_bounds = srs::encompassing_bounds_transfer(
        dataset_srs, ecef_srs, dataset_bounds);
    const auto space = octree::Space::earth();
    const auto root_node = expect(
        space.find_smallest_node_encompassing_bounds(ecef_bounds),
        "Dataset is outside the octree root node.");

    tbb::concurrent_vector<octree::Id> concurrent_targets;

    tbb::task_group tg;
    std::function<void(octree::Id)> process_node;

    tbb::enumerable_thread_specific<std::shared_ptr<OGRCoordinateTransformation>> transform_ecef_dataset([&]() {
        auto local_ecef_srs = ecef_srs;
        auto local_dataset_srs = dataset_srs;
        auto transform = srs::transformation(local_ecef_srs, local_dataset_srs);
        return transform;
    });

    process_node = [&](octree::Id node) {
        // Check if node intersects with ecef bounds of dataset
        const auto node_bounds = space.get_node_bounds(node);
        if (!radix::geometry::intersect(node_bounds, ecef_bounds)) {
            return;
        }

        // Check if node bounds in dataset srs intersect with dataset
        const auto node_bounds_dataset_srs = srs::encompassing_bounds_transfer(
            &*transform_ecef_dataset.local(), node_bounds, 7, 3);
        if (!radix::geometry::intersect(node_bounds_dataset_srs, dataset_bounds)) {
            return;
        }

        if (node.level() < target_level) {
            if (auto children = node.children(); children.has_value()) {
                for (const auto &child : *children) {
                    tg.run([&, child] { process_node(child); });
                }
            }
        } else if (node.level() == target_level) {
            concurrent_targets.push_back(node);
        }
    };

    tg.run([&] { process_node(root_node); });
    tg.wait(); // Wait for all tasks

    std::vector<octree::Id> target_nodes;
    target_nodes.assign(concurrent_targets.begin(), concurrent_targets.end());

    ProgressIndicator progress(target_nodes.size());
    std::jthread progress_thread = progress.start_monitoring();

    // Clone dataset and SRSs for each thread
    tbb::enumerable_thread_specific<Dataset> local_dataset([&]() {
        return dataset.clone();
    });
    tbb::enumerable_thread_specific<std::unique_ptr<OGRSpatialReference>> local_ecef_srs([&]() {
        return srs::clone(ecef_srs);
    });
    tbb::enumerable_thread_specific<std::unique_ptr<OGRSpatialReference>> local_texture_srs([&]() {
        return srs::clone(texture_srs);
    });
    tbb::enumerable_thread_specific<std::unique_ptr<OGRSpatialReference>> local_mesh_srs([&]() {
        return srs::clone(mesh_srs);
    });

    // Decrease log level to error to avoid excessive logging during mesh building
    auto logger = Log::get_logger();
    const auto original_level = logger->level();
    const auto new_level = spdlog::level::err;
    // Only set to if it's more restrictive than current level
    if (new_level >= original_level) {
        logger->set_level(new_level);
    }

    tbb::parallel_for(size_t(0), target_nodes.size(), [&](size_t i) {
        const auto &node = target_nodes[i];
        if (!overwrite_existing && storage.has_node(node)) {
            progress.task_finished(); // TODO: correctly handle virtual nodes
            return;
        }

        auto &dataset = local_dataset.local();
        auto &ecef_srs = *local_ecef_srs.local();
        auto &texture_srs = *local_texture_srs.local();
        auto &mesh_srs = *local_mesh_srs.local();

        const auto node_bounds = space.get_node_bounds(node);
        auto mesh_result = terrainbuilder::build_patch(
            dataset,
            ecef_srs,
            node_bounds,
            texture_srs,
            texture_base_path,
            mesh_srs);

        if (mesh_result.has_value()) {
            const auto mesh = std::move(mesh_result.value());
            mesh::validate(mesh);
            storage.write_node(node, mesh);
        }

        progress.task_finished();
    });

    // Restore original level
    logger->set_level(original_level);

    progress_thread.join();
    storage.save_or_create_index();
}
}
