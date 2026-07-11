#include "cli.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <libassert/assert.hpp>

using namespace cli;

Args cli::parse(int argc, const char * const * argv) {
    DEBUG_ASSERT(argc >= 0);

    Args args;
    CLI::App app{"mesh merger"};
    app.positionals_at_end(false);
    app.allow_windows_style_options(false);

    app.add_option("--input", args.mesh_paths, "Meshes to to merge together")
        ->check(CLI::ExistingFile)
        ->required();

    app.add_option("--output", args.output_path, "Path to write the merged mesh to")
        ->required();

    app.add_option("--epsilon", args.epsilon, "Epsilon for vertex deduplication")
        ->check(CLI::Range(std::numeric_limits<double>::min(), std::numeric_limits<double>::max()));

    const std::map<std::string, spdlog::level::level_enum>
        log_level_names{
            {"off", spdlog::level::level_enum::off},
            {"critical", spdlog::level::level_enum::critical},
            {"error", spdlog::level::level_enum::err},
            {"warn", spdlog::level::level_enum::warn},
            {"info", spdlog::level::level_enum::info},
            {"debug", spdlog::level::level_enum::debug},
            {"trace", spdlog::level::level_enum::trace}};
    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val(spdlog::level::level_enum::info);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        exit(app.exit(e));
    }

    return args;
}
