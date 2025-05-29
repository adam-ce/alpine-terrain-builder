#include "cli.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

using namespace cli;

Args cli::parse(int argc, const char * const * argv) {
    assert(argc >= 0);

    CLI::App app{"terrainsimplify"};
    app.allow_windows_style_options();
    // argv = app.ensure_utf8(argv);
    
    std::vector<std::filesystem::path> input_paths;
    app.add_option("--input", input_paths, "Paths to tiles that should be merged")
        ->required()
        ->expected(-1)
        ->check(CLI::ExistingFile);

    std::filesystem::path output_path;
    app.add_option("--output", output_path, "Path to output the merged tile to")
        ->required();

    bool no_mesh_simplification = false;
    app.add_flag("--no-simplify", no_mesh_simplification, "Disable mesh simplification");

    std::optional<double> simplification_vertex_ratio;
    app.add_option("--simplify-vertex-ratio", simplification_vertex_ratio, "Target mesh vertex simplification factor")
        ->check(CLI::Range(0.0, 1.0))
        ->excludes("--no-simplify");

    std::optional<double> simplification_edge_ratio;
    app.add_option("--simplify-edge-ratio", simplification_edge_ratio, "Target mesh edge simplification factor")
        ->check(CLI::Range(0.0, 1.0))
        ->excludes("--no-simplify");

    std::optional<double> simplification_face_ratio;
    app.add_option("--simplify-face-ratio", simplification_face_ratio, "Target mesh face simplification factor")
        ->check(CLI::Range(0.0, 1.0))
        ->excludes("--no-simplify");

    std::optional<double> simplification_target_error_relative;
    app.add_option("--simplify-error-relative", simplification_target_error_relative, "Relative mesh simplification error bound")
        ->check(CLI::Range(0.0, 1.0))
        ->excludes("--no-simplify");

    std::optional<double> simplification_target_error_absolute;
    app.add_option("--simplify-error-absolute", simplification_target_error_absolute, "Absolute mesh simplification error bound")
        ->check(CLI::PositiveNumber)
        ->excludes("--no-simplify");

    std::vector<unsigned int> target_texture_resolution;
    app.add_option("--texture-resolution", target_texture_resolution, "Resolution of merged mesh texture")
        ->check(CLI::PositiveNumber)
        ->expected(2);

    bool save_intermediate_meshes = false;
    app.add_flag("--save-debug-meshes", save_intermediate_meshes, "Output intermediate meshes");

    spdlog::level::level_enum log_level = spdlog::level::level_enum::info;
    const std::map<std::string, spdlog::level::level_enum> log_level_names{
        {"off", spdlog::level::level_enum::off},
        {"critical", spdlog::level::level_enum::critical},
        {"error", spdlog::level::level_enum::err},
        {"warn", spdlog::level::level_enum::warn},
        {"info", spdlog::level::level_enum::info},
        {"debug", spdlog::level::level_enum::debug},
        {"trace", spdlog::level::level_enum::trace}};
    app.add_option("--verbosity", log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case));

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }
    Args args;
    args.input_paths = input_paths;
    args.output_path = output_path;
    args.log_level = log_level;
    args.save_intermediate_meshes = save_intermediate_meshes;
    if (target_texture_resolution.size() == 2) {
        args.target_texture_resolution = {target_texture_resolution[0], target_texture_resolution[1]};
    }
    if (!no_mesh_simplification) {
        SimplificationArgs simplification_args = {};
        if (simplification_vertex_ratio.has_value()) {
            simplification_args.stop_condition.push_back(simplify::VertexRatio { .ratio=simplification_vertex_ratio.value() });
        } 
        if (simplification_edge_ratio.has_value()) {
            simplification_args.stop_condition.push_back(simplify::EdgeRatio { .ratio=simplification_edge_ratio.value() });
        } 
        if (simplification_face_ratio.has_value()) {
            simplification_args.stop_condition.push_back(simplify::FaceRatio { .ratio=simplification_face_ratio.value() });
        } 
        if (simplification_target_error_relative.has_value()) {
            simplification_args.stop_condition.push_back(simplify::RelativeError { .error_bound = simplification_target_error_relative.value() });
        }
        if (simplification_target_error_absolute.has_value()) {
            simplification_args.stop_condition.push_back(simplify::AbsoluteError { .error_bound=simplification_target_error_absolute.value() });
        }
        if (simplification_args.stop_condition.empty()) {
            simplification_args.stop_condition.push_back(simplify::EdgeRatio { .ratio=1.0/args.input_paths.size() });
        }
        args.simplification = simplification_args;
    }

    return args;
}
