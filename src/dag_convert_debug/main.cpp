#include <cstdlib>
#include <filesystem>

#include "cli.h"
#include "codec.h"
#include "dag_node.h"
#include "log.h"
#include "mesh/io.h"
#include "mesh/storage.h"
#include "mesh/storage.h"
#include "ProgressIndicator.h"
#include "storage.h"
#include "utils.h"

namespace {

void export_node(const cli::Args &args) {
    if (args.input_path.extension() != ".dag") {
        LOG_ERROR("Expected a .dag input file, got {}", args.input_path);
        return;
    }
    std::filesystem::path node_path = args.input_path;
    node_path.replace_extension();
    const dag::codec::ClusterBatch codec;
    const auto load_result = codec.read(node_path);
    if (!load_result) {
        LOG_ERROR("Failed to load node from {}: {}", args.input_path, load_result.error().to_string());
        return;
    }

    const mesh::Simple mesh = clustering_to_mesh(load_result.value().clustering);

    const auto save_result = mesh::io::save_to_path(mesh, args.output_path);
    if (!save_result) {
        LOG_ERROR("Failed to save mesh to {}: {}", args.output_path, save_result.error().to_string());
    }
}

void export_storage(const cli::Args &args) {
    auto input_result = dag::storage::open_folder_indexed(args.input_path);
    if (!input_result) {
        LOG_ERROR(
            "Failed to open input storage {}: {}",
            args.input_path,
            input_result.error().to_string());
        return;
    }
    const dag::storage::IndexedStorage input_storage = std::move(input_result.value());

    mesh::storage::OpenOptions options;
    options.preferred_extension = ".glb";
    auto output_result = mesh::storage::open_folder(
        args.output_path,
        std::move(options));
    if (!output_result) {
        LOG_ERROR(
            "Failed to open output storage {}: {}",
            args.output_path,
            output_result.error().to_string());
        return;
    }
    mesh::storage::Storage output_storage = std::move(output_result.value());
    output_storage.settings().allow_overwrite = true;

    size_t exported_count = 0;

    ProgressIndicator progress(input_storage.index().size());
    auto progress_thread = progress.start_monitoring();

    for (const auto &[id, status] : input_storage.index()) {
        if (status == store::NodeStatus::Virtual) {
            progress.task_finished();
            continue;
        }

        const auto load_result = input_storage.load(id);
        if (!load_result) {
            LOG_ERROR(
                "Failed to load node {}: {}",
                id,
                load_result.error().to_string());
            progress.task_finished();
            continue;
        }

        const mesh::Simple mesh = clustering_to_mesh(load_result.value().clustering);

        const auto save_result = output_storage.save(id, mesh);
        if (!save_result) {
            LOG_ERROR(
                "Failed to save mesh for node {}: {}",
                id,
                save_result.error().to_string());
            progress.task_finished();
            continue;
        }

        exported_count++;
        progress.task_finished();
    }

    progress_thread.join();

    const auto index_result = output_storage.save_or_create_index();
    if (!index_result) {
        LOG_ERROR(
            "Failed to save index for {}: {}",
            args.output_path,
            index_result.error().to_string());
    }

    LOG_INFO("Exported {} debug meshes to {}", exported_count, args.output_path);
}

} // namespace

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    if (std::filesystem::is_directory(args.input_path)) {
        export_storage(args);
    } else {
        export_node(args);
    }

    return EXIT_SUCCESS;
}
