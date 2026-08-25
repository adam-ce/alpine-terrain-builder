#include <cstdlib>
#include <exception>
#include <filesystem>

#include "cli.h"
#include "build.h"
#include "log.h"
#include "mesh/storage.h"
#include "storage.h"
#include "store/describe_error.h"
#include "sf/Error.h"
#include "ContinuationMode.h"

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        auto input_result = mesh::storage::open_folder_indexed(args.input_path);
        if (!input_result.has_value()) {
            LOG_ERROR(
                "Failed to open input dataset {}: {}",
                args.input_path,
                store::describe_error(input_result.error()));
            return EXIT_FAILURE;
        }
        auto output_result = dag::storage::open_folder_indexed(args.output_path);
        if (!output_result.has_value()) {
            LOG_ERROR(
                "Failed to open output dataset {}: {}",
                args.output_path,
                store::describe_error(output_result.error()));
            return EXIT_FAILURE;
        }
        const mesh::storage::IndexedStorage input_storage = std::move(input_result.value());
        dag::storage::IndexedStorage output_storage = std::move(output_result.value());
        output_storage.settings().allow_overwrite = args.continuation_mode == ContinuationMode::Overwrite;

        dag::BuildOptions options{
            .clusters_per_partition = args.clusters_per_partition,
            .target_ratio = args.target_ratio,
            .relative_target_error = args.target_error,
            .texture_options = {
                .atlas = {.padding = args.texture_gutter},
                .bake = {.reprojection = {.gutter = args.texture_gutter}},
                .sizing = args.sizing_options,
                .charting = args.charting,
                .allow_texture_reuse = args.allow_texture_reuse,
            },
            .root_node = args.root_node,
            .include_mode = args.include_mode,
            .write_debug_meshes = args.write_debug_meshes,
            .parallelize = args.parallelize,
            .continuation_mode = args.continuation_mode
        };
        // If the user specified neither target, fall back to a default error.
        if (!args.target_ratio && !args.target_error) {
            options.relative_target_error = 0.001f;
        }

        const auto build_result = dag::build_levels(
            input_storage,
            output_storage,
            options,
            args.level_range);
        if (!build_result.has_value()) {
            LOG_ERROR("Invalid Structura Fundamentalis input: {}", sf::describe_error(build_result.error()));
            return EXIT_FAILURE;
        }
        const auto index_result = output_storage.save_index();
        if (!index_result.has_value()) {
            LOG_ERROR(
                "Failed to save output index in {}: {}",
                args.output_path,
                store::describe_error(index_result.error()));
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
