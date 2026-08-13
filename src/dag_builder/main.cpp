#include <cstdlib>
#include <exception>
#include <filesystem>

#include "cli.h"
#include "build.h"
#include "log.h"
#include "octree/storage/Storage.h"
#include "octree/storage/MeshStorage.h"
#include "octree/storage/open.h"
#include "storage.h"
#include "ContinuationMode.h"

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        const octree::IndexedMeshStorage input_storage = octree::open_folder_indexed(args.input_path);
        octree::IndexedDagStorage output_storage = octree::open_folder_indexed<dag::ClusterBatch>(args.output_path);
        output_storage.settings().allow_overwrite = args.continuation_mode == ContinuationMode::Overwrite;

        dag::BuildOptions options{
            .clusters_per_partition = args.clusters_per_partition,
            .target_ratio = args.target_ratio,
            .relative_target_error = args.target_error,
            .merge_options = {
                .uv_unwrap_algorithm = args.uv_unwrap_algorithm,
                .allow_texture_reuse = args.allow_texture_reuse,
            },
            .bake_options = args.bake_options,
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

        dag::build_levels(input_storage, output_storage, options, args.level_range);
        const auto index_result = output_storage.save_index();
        if (!index_result.has_value()) {
            LOG_ERROR("Failed to save output index in {}: {}", args.output_path, index_result.error());
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}
