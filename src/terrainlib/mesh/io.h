#pragma once

#include <filesystem>

#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/io/options.h"
#include "mesh/io/error.h"

namespace mesh {
namespace io {

tl::expected<SimpleMesh, LoadMeshError> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options = {});

tl::expected<void, SaveMeshError> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions& options = {});

}
}
