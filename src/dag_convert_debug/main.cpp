#include <cstdlib>
#include <filesystem>

#include "cli.h"
#include "dag_node.h"
#include "encoded.h"
#include "log.h"
#include "mesh/io.h"
#include "octree/storage/MeshStorage.h"
#include "octree/storage/codec/DefaultCodec.h"
#include "octree/storage/open.h"
#include "ProgressIndicator.h"
#include "storage.h"
#include "utils.h"

namespace {

void export_node_file(const cli::Args &args) {
    const auto load_result = octree::ZppBitsCodec<dag::ClusterBatch>::load_from_path(args.input_path);
    if (!load_result.has_value()) {
        LOG_ERROR("Failed to load node from {}: {}", args.input_path, load_result.error());
        return;
    }

    const mesh::Simple mesh = clustering_to_mesh(load_result.value().clustering, true);

    const auto save_result = mesh::io::save_to_path(mesh, args.output_path);
    if (!save_result.has_value()) {
        LOG_ERROR("Failed to save mesh to {}: {}", args.output_path, save_result.error().description());
    }
}

void export_storage_folder(const cli::Args &args) {
    const octree::IndexedDagStorage input_storage = octree::open_folder_indexed<dag::ClusterBatch>(args.input_path);

    octree::MeshStorage output_storage = octree::open_folder<mesh::Simple, octree::MeshCodec>(
        args.output_path,
        false,
        octree::OpenOptions{.preferred_extension_with_dot = ".glb"});
    output_storage.settings().allow_overwrite = true;

    size_t exported_count = 0;

    ProgressIndicator progress(input_storage.index().size());
    auto progress_thread = progress.start_monitoring();

    for (const auto &[id, status] : input_storage.index()) {
        if (status == octree::NodeStatus::Virtual) {
            progress.task_finished();
            continue;
        }

        const auto load_result = input_storage.load(id);
        if (!load_result.has_value()) {
            LOG_ERROR("Failed to load node {}: {}", id, load_result.error());
            progress.task_finished();
            continue;
        }

        const mesh::Simple mesh = clustering_to_mesh(load_result.value().clustering);

        const auto save_result = output_storage.save(id, mesh);
        if (!save_result.has_value()) {
            LOG_ERROR("Failed to save mesh for node {}: {}", id, save_result.error().description());
            progress.task_finished();
            continue;
        }

        exported_count++;
        progress.task_finished();
    }

    progress_thread.join();

    const auto index_result = output_storage.save_or_create_index();
    if (!index_result.has_value()) {
        LOG_ERROR("Failed to save index for {}: {}", args.output_path, index_result.error());
    }

    LOG_INFO("Exported {} debug meshes to {}", exported_count, args.output_path);
}

} // namespace

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    if (std::filesystem::is_directory(args.input_path)) {
        export_storage_folder(args);
    } else {
        export_node_file(args);
    }

    return EXIT_SUCCESS;
}
