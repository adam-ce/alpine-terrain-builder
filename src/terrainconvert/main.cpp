#include <filesystem>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <tl/expected.hpp>

#include "mesh/terrain_mesh.h"
#include "mesh/io.h"
#include "log.h"
#include "cli.h"

void run(const cli::Args& args) {
    LOG_INFO("Loading input mesh...");
    const tl::expected<SimpleMesh, io::LoadMeshError> load_result = io::load_mesh_from_path(args.input_path);
    if (!load_result.has_value()) {
        LOG_ERROR("Failed to load mesh: {}", load_result.error().description());
        return;
    }
    SimpleMesh mesh = load_result.value();

    if (args.texture_resolution.has_value()) {
        if (mesh.texture.has_value()) {
            cv::Mat& texture = mesh.texture.value();
            glm::uvec2 target_resolution = args.texture_resolution.value();
            LOG_INFO("Resizing mesh texture to {}x{}", target_resolution.x, target_resolution.y);
            cv::resize(texture, texture, cv::Size(target_resolution.x, target_resolution.y), cv::INTER_LINEAR);
        } else {
            LOG_WARN("--texture-resolution specified but no texture present");
        }
    }

    LOG_INFO("Writing output mesh...");
    const tl::expected<void, io::SaveMeshError> save_result = io::save_mesh_to_path(args.output_path, mesh);
    if (!save_result.has_value()) {
        LOG_ERROR("Failed to save mesh: {}", save_result.error().description());
        return;
    }
}

int main(int argc, char **argv) {
    const cli::Args args = cli::parse(argc, argv);
    Log::init(args.log_level);

    run(args);
}
