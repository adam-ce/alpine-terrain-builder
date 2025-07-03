#include "cli.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

using namespace cli;

Args cli::parse(int argc, const char * const * argv) {
    DEBUG_ASSERT(argc >= 0);

    CLI::App app{"terrainmerger"};
    app.allow_windows_style_options();
    // argv = app.ensure_utf8(argv);
    
    std::vector<std::filesystem::path> input_paths;
    app.add_option("--input", input_paths, "Paths to datasets that should be merged")
        ->required()
        ->expected(-1)
        ->check(CLI::ExistingDirectory);

    std::filesystem::path output_path;
    app.add_option("--output", output_path, "Path to output write the merged dataset to")
        ->required();

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

    return args;
}
