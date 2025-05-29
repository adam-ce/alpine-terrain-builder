#include <CLI/CLI.hpp>

#include "cli.h"

using namespace cli;

Args cli::parse(int argc, const char * const* argv) {
    assert(argc >= 0);
    CLI::App app{"terrainconvert"};
    
    std::filesystem::path input_path;
    app.add_option("--input", input_path, "Path of tile to be converted")
        ->required()
        ->check(CLI::ExistingFile);

    std::filesystem::path output_path;
    app.add_option("--output", output_path, "Path to output the converted tile to")
        ->required();

    std::vector<unsigned int> texture_resolution;
    app.add_option("--texture-resolution", texture_resolution, "Resolution of output mesh texture")
        ->check(CLI::PositiveNumber)
        ->expected(2);

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
    args.input_path = input_path;
    args.output_path = output_path;
    args.log_level = log_level;
    if (texture_resolution.size() == 2) {
        args.texture_resolution = {texture_resolution[0], texture_resolution[1]};
    }

    return args;
}
