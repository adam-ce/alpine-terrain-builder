#pragma once

#include <filesystem>
#include <vector>
#include <memory>

#include <cgltf_write.h>
#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "mesh/io/options.h"

namespace mesh {
namespace io {
namespace gltf {

using RawMesh = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

tl::expected<SimpleMesh, LoadMeshError> load_from_path(
    const std::filesystem::path &path,
    const LoadOptions &options = {});
tl::expected<SimpleMesh, LoadMeshError> load_from_raw(
    const RawMesh &mesh,
    const LoadOptions &options = {});

tl::expected<void, SaveMeshError> save_to_path(
    const SimpleMesh &mesh,
    const std::filesystem::path &path,
    const SaveOptions &options = {});
// tl::expected<SimpleMesh, LoadMeshError> save_to_raw(const RawMesh &mesh, const SaveOptions &options = {});

tl::expected<RawMesh, cgltf_result> load_raw_from_path(const std::filesystem::path &path);

} // namespace gltf
} // namespace io
} // namespace mesh
