#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <tl/expected.hpp>

#include "mesh/io.h"

namespace octree {

struct MeshCodec {
    using value_type = mesh::Simple;
    using load_error = mesh::io::LoadMeshError;
    using save_error = mesh::io::SaveMeshError;

    static tl::expected<value_type, load_error> load_from_path(const std::filesystem::path& path) noexcept {
        return mesh::io::load_from_path(path);
    }

    static tl::expected<void, save_error> save_to_path(const value_type& value, const std::filesystem::path& path) noexcept {
        return mesh::io::save_to_path(value, path);
    }

    static load_error file_not_found() noexcept {
        return mesh::io::LoadMeshErrorKind::FileNotFound;
    }

    static std::vector<std::string_view> supported_extensions() {
        return {".terrain", ".glb", ".gltf"};
    }
};

} // namespace octree
