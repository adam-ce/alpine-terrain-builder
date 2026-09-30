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

// Standard library
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// External libraries
#include <fmt/core.h>
#include <fmt/format.h>
#include <gdal_priv.h>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/norm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <spdlog/spdlog.h>
#include <expected>

// Internal headers
#include "log.h"
#include "hash_utils.h"
#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "mesh/io.h"
