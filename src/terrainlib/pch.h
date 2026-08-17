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
