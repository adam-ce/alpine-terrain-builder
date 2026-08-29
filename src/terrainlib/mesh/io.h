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
