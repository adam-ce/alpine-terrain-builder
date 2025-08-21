#include <span>
#include <vector>
#include <functional>
#include <algorithm>
#include <filesystem>

#include "cli.h"
#include "log.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io.h"
#include "mesh/merging/mapping.h"
#include "mesh/holes.h"

SimpleMesh merge(const std::span<SimpleMesh> meshes, const std::optional<double> epsilon) {
    std::vector<std::reference_wrapper<const SimpleMesh>> mesh_refs;
    mesh_refs.reserve(meshes.size());
    for (const SimpleMesh &mesh : meshes) {
        mesh_refs.push_back(mesh);
    }

    mesh::merging::VertexMapping mapping;
    if (epsilon.has_value()) {
        mapping = mesh::merging::create_mapping(mesh_refs, epsilon.value());
    } else {
        mapping = mesh::merging::create_mapping(mesh_refs);
    }

    SimpleMesh merged = mesh::merging::apply_mapping(mesh_refs, mapping);
    mesh::fill_holes_on_merge_border(merged, mapping);
    return merged;
}

void run(const cli::Args &args) {
    std::vector<SimpleMesh> meshes;
    meshes.reserve(args.mesh_paths.size());
    for (const std::filesystem::path& mesh_path : args.mesh_paths) {
        LOG_INFO("Loading mesh from {}", mesh_path);
        auto result = mesh::io::load_from_path(mesh_path);
        if (!result.has_value()) {
            LOG_ERROR_AND_EXIT("Failed to load mesh to {} due to {}", mesh_path, result.error().description());
        }
        const SimpleMesh mesh = result.value();
        meshes.push_back(mesh);
    }

    LOG_INFO("Merging meshes");
    const SimpleMesh merged = merge(meshes, args.epsilon);

    LOG_INFO("Saving mesh to {}", args.output_path);
    auto result = mesh::io::save_to_path(merged, args.output_path);
    if (!result.has_value()) {
        LOG_ERROR_AND_EXIT("Failed to save mesh to {} due to {}", args.output_path, result.error().description());
    }
}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    const std::string arg_str = std::accumulate(argv, argv + argc, std::string(),
                                                [](const std::string &acc, const char *arg) {
                                                    return acc + (acc.empty() ? "" : " ") + arg;
                                                });
    LOG_DEBUG("Running with: {}", arg_str);

    run(args);
    return 0;
}
