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

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);

    Log::init(args.log_level);

    try {
        const octree::IndexedMeshStorage input_storage = octree::open_folder_indexed(args.input_path);
        octree::IndexedDagStorage output_storage = octree::open_folder_indexed<dag::ClusterBatch>(args.output_path);
        output_storage.settings().allow_overwrite = args.overwrite;

        const dag::BuildOptions options{
            .clusters_per_partition = args.clusters_per_partition,
            .target_ratio = args.target_ratio,
            .uv_unwrap_algorithm = args.uv_unwrap_algorithm,
            .root_node = args.root_node,
            .write_debug_meshes = args.write_debug_meshes,
            .resume = !args.overwrite,
        };

        dag::build_levels(input_storage, output_storage, options, args.level_range);
        output_storage.save_index();

        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        LOG_ERROR("{}", e.what());
        return EXIT_FAILURE;
    }
}