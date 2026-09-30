/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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

#include <filesystem>

#include "Error.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io/options.h"

namespace mesh::io {

Expected<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options = {});

Expected<void> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions& options = {});

}
