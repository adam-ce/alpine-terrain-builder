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
            .texture_options = {
                .atlas = {.padding = args.texture_gutter},
                .bake = {.reprojection = {.gutter = args.texture_gutter}},
                .sizing = args.sizing_options,
                .charting = args.charting,
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

        dag::build_levels(input_storage, output_storage, options, args.level_range);
        output_storage.save_index();

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}