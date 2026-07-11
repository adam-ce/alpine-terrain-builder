#pragma once

#include <filesystem>
#include <vector>

#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "mesh/io/options.h"

namespace mesh::io::terrain {

tl::expected<SimpleMesh, LoadMeshError> load_from_path(const std::filesystem::path &path, const LoadOptions &options = {});
tl::expected<SimpleMesh, LoadMeshError> load_from_buffer(const std::span<const uint8_t> buffer, const LoadOptions &options = {});

tl::expected<void, SaveMeshError> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions &options = {});
tl::expected<std::vector<uint8_t>, SaveMeshError> save_to_buffer(const SimpleMesh &mesh, const SaveOptions &options = {});

} // namespace mesh::io::terrain
