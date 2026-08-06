#include "cli.h"

#include <CLI/CLI.hpp>
#include <libassert/assert.hpp>

#include <filesystem>
#include <map>
#include <string>

using namespace cli;

namespace {

const std::map<std::string, spdlog::level::level_enum> log_level_names{
    {"off", spdlog::level::level_enum::off},
    {"critical", spdlog::level::level_enum::critical},
    {"error", spdlog::level::level_enum::err},
    {"warn", spdlog::level::level_enum::warn},
    {"info", spdlog::level::level_enum::info},
    {"debug", spdlog::level::level_enum::debug},
    {"trace", spdlog::level::level_enum::trace}};

} // namespace

Args cli::parse(int argc, const char *const *argv) {
    DEBUG_ASSERT(argc >= 0);
    CLI::App app{"dag_convert_debug"};

    Args args{
        .input_path = {},
        .output_path = {},
        .log_level = spdlog::level::level_enum::info,
    };

    app.add_option("input,--input", args.input_path, "Path to a DAG storage folder, or a single node file")
        ->required()
        ->check(CLI::ExistingPath);

    app.add_option("output,--output", args.output_path, "Path to write debug meshes to: a folder if --input is a folder, or a mesh file if --input is a file");

    app.add_option("--verbosity", args.log_level, "Verbosity level of logging")
        ->transform(CLI::CheckedTransformer(log_level_names, CLI::ignore_case))
        ->default_val(args.log_level);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        std::exit(app.exit(e));
    }

    if (args.output_path.empty()) {
        auto input_path = args.input_path.lexically_normal();
        if (std::filesystem::is_directory(input_path)) {
            auto stem = input_path.filename().string();
            args.output_path = input_path.parent_path() / fmt::format("{}-debug", stem);
        } else {
            auto stem = input_path.stem().string();
            auto ext = input_path.extension().string();
            if (!ext.empty()) {
                ext = ".glb";
            }
            args.output_path = input_path.parent_path() / fmt::format("{}-debug{}", stem, ext);
        }
    }

    return args;
}
