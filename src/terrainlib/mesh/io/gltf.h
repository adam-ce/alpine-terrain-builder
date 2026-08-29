#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <cgltf_write.h>

#include "Error.h"
#include "mesh/SimpleMesh.h"
#include "mesh/io/options.h"

namespace mesh::io::gltf {

using RawMesh = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

Expected<SimpleMesh> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions &options = {});
Expected<SimpleMesh> load_from_raw(
    const RawMesh &mesh,
    const LoadOptions &options = {});

Expected<void> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions &options = {});

Expected<RawMesh> load_raw_from_path(const std::filesystem::path &path);

} // namespace mesh::io::gltf
