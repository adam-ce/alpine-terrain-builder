/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <CLI/CLI.hpp>

#include "cli.h"

using namespace cli;

Args cli::parse(int argc, const char * const* argv) {
    DEBUG_ASSERT(argc >= 0);
    CLI::App app{"mesh_convert"};
    
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
