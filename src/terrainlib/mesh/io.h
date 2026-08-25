#pragma once

#include <filesystem>

#include <expected>

#include "mesh/SimpleMesh.h"
#include "mesh/io/options.h"
#include "mesh/io/error.h"

namespace mesh::io {

std::expected<SimpleMesh, LoadMeshError> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions& options = {});

std::expected<void, SaveMeshError> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions& options = {});

}
