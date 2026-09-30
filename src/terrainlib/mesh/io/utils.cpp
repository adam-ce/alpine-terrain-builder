/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include "mesh/io/utils.h"

namespace mesh::io::utils {

std::filesystem::path create_parent_directories(const std::filesystem::path &path) {
    const std::filesystem::path parent_path = std::filesystem::absolute(path).parent_path();
    std::filesystem::create_directories(parent_path);
    return parent_path;
}

} // namespace mesh::io::utils
