#include "cli.h"
#include "mesh/convert.h"
#include "log.h"
#include "mesh/merge.h"
#include "mesh/io.h"
#include "simplify.h"
#include "uv_map.h"
#include "mesh/validate.h"

std::vector<SimpleMesh> load_meshes_from_path(std::span<const std::filesystem::path> paths, const bool print_errors = true) {
    std::vector<SimpleMesh> meshes;
    meshes.reserve(paths.size());
    for (const std::filesystem::path &path : paths) {
        auto result = mesh::io::load_from_path(path);
        if (!result.has_value()) {
            const mesh::io::LoadMeshError error = result.error();
            if (print_errors) {
                LOG_ERROR("Failed to load mesh from {}: {}", path, error.description());
            }
            exit(EXIT_FAILURE);
        }

        const SimpleMesh mesh = std::move(result.value());
        mesh::validate(mesh);
        mesh::validate(convert::mesh2cgal(mesh));
        meshes.push_back(mesh);
    }

    return meshes;
}

uv_map::UvMap parameterize_mesh(SimpleMesh &mesh) {
    const tl::expected<uv_map::UvMap, uv_map::UvParameterizationError> result =
        uv_map::parameterize_mesh(mesh, uv_map::Algorithm::DiscreteConformalMap, uv_map::Border::Circle);
    if (result) {
        return result.value();
    } else {
        const uv_map::UvParameterizationError error = result.error();
        LOG_ERROR("Failed to parameterize merged mesh due to '{}'", error.description());
        exit(EXIT_FAILURE);
    }
}

SimpleMesh simplify_mesh(const SimpleMesh &mesh, const cli::SimplificationArgs &args) {
    const simplify::Result result = simplify::simplify_mesh(mesh, args.stop_condition);
    const SimpleMesh &simplified_mesh = result.mesh;

    const size_t initial_vertex_count = mesh.positions.size();
    const size_t initial_face_count = mesh.triangles.size();
    const size_t simplified_vertex_count = simplified_mesh.positions.size();
    const size_t simplified_face_count = simplified_mesh.triangles.size();

    LOG_DEBUG("Simplified mesh to {}/{} vertices and {}/{} faces",
              simplified_vertex_count, initial_vertex_count,
              simplified_face_count, initial_face_count);

    return result.mesh;
}

void run(const cli::Args &args) {
    LOG_INFO("Loading meshes...");
    std::vector<SimpleMesh> meshes = load_meshes_from_path(args.input_paths);

    const bool meshes_have_uvs = std::all_of(meshes.begin(), meshes.end(), [](const SimpleMesh &mesh) { return mesh.has_uvs(); });
    const bool meshes_have_textures = std::all_of(meshes.begin(), meshes.end(), [](const SimpleMesh &mesh) { return mesh.has_texture(); });
    
    const std::optional<glm::uvec2> texture_size = (!meshes.empty() && meshes[0].texture.has_value())
                                                       ? std::optional<glm::uvec2>{convert::cv2glm(meshes[0].texture.value().size())}
                                                       : std::nullopt;
    const glm::uvec2 target_texture_size = args.target_texture_resolution.has_value() ?
        args.target_texture_resolution.value() : (texture_size.has_value() ? texture_size.value() : glm::uvec2(256));

    LOG_INFO("Merging meshes...");
    merge::VertexMapping vertex_mapping;
    SimpleMesh merged_mesh = merge::merge_meshes(meshes, vertex_mapping);
    if (args.save_intermediate_meshes) {
        const std::filesystem::path merged_mesh_path = std::filesystem::path(args.output_path).replace_extension(".merged.glb");
        LOG_DEBUG("Saving merged mesh to {}", merged_mesh_path);
        mesh::io::save_to_path(merged_mesh, merged_mesh_path, mesh::io::SaveOptions{.name = "merged"});
    }

    if (meshes_have_uvs) {
        LOG_INFO("Calculating uv mapping...");
        const uv_map::UvMap uv_map = parameterize_mesh(merged_mesh);
        merged_mesh.uvs = uv_map::decode_uv_map(uv_map, merged_mesh.vertex_count());

        if (meshes_have_textures) {
            LOG_INFO("Merging textures...");
            merged_mesh.texture = uv_map::merge_textures(meshes, merged_mesh, vertex_mapping, uv_map, target_texture_size * glm::uvec2(2));

            if (args.save_intermediate_meshes) {
                const std::filesystem::path merged_mesh_path = std::filesystem::path(args.output_path).replace_extension(".textured.glb");
                LOG_DEBUG("Saving merged mesh to {}", merged_mesh_path);
                mesh::io::save_to_path(merged_mesh, merged_mesh_path, mesh::io::SaveOptions{.name = "textured"});
            }
        }
    }

    SimpleMesh simplified_mesh;
    if (args.simplification) {
        LOG_INFO("Simplifying merged mesh...");
        simplified_mesh = simplify_mesh(merged_mesh, args.simplification.value());

        if (merged_mesh.texture.has_value()) {
            LOG_INFO("Simplifying merged texture...");
            simplified_mesh.texture = simplify::simplify_texture(merged_mesh.texture.value(), target_texture_size);
        }

        if (args.save_intermediate_meshes) {
            const std::filesystem::path simplified_mesh_path = std::filesystem::path(args.output_path).replace_extension(".simplified.glb");
            LOG_DEBUG("Saving simplified mesh to {}", simplified_mesh_path.string());
            mesh::io::save_to_path(simplified_mesh, simplified_mesh_path, mesh::io::SaveOptions{.name = "simplified"});
        }
    } else {
        simplified_mesh = merged_mesh;
    }

    LOG_INFO("Saving final mesh...");
    mesh::io::save_to_path(simplified_mesh, args.output_path);

    LOG_INFO("Done");
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
}
