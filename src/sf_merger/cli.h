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

#pragma once

#include <vector>
#include <filesystem>

#include <spdlog/spdlog.h>

namespace cli {

struct BaseArgs {
    spdlog::level::level_enum log_level;
};

/*
enum class MergeAlgorithm {
    Combine,
    Masked,
    Project
};
*/

struct MergeArgs : public BaseArgs {
    std::filesystem::path base_path;
    std::filesystem::path new_path;
    std::filesystem::path output_path;
    std::optional<std::filesystem::path> mask_path;
    // MergeAlgorithm algorihm;
    bool overwrite_output;
};

struct CutArgs : public BaseArgs {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::filesystem::path mask_path;
    bool keep_inside;
};

using Args = std::variant<MergeArgs, CutArgs>;

Args parse(int argc, const char *const *argv);

}
