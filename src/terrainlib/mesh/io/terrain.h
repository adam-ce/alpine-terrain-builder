#pragma once

#include <filesystem>
#include <vector>

#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/io/error.h"
#include "mesh/io/options.h"
#include "mesh/io/texture.h"

namespace mesh::io::terrain {

tl::expected<SimpleMesh, LoadMeshError> load_from_path(const std::filesystem::path &path, const LoadOptions &options = {});
tl::expected<SimpleMesh, LoadMeshError> load_from_buffer(const std::span<const uint8_t> buffer, const LoadOptions &options = {});

tl::expected<void, SaveMeshError> save_to_path(const SimpleMesh &mesh, const std::filesystem::path &path, const SaveOptions &options = {});
tl::expected<std::vector<uint8_t>, SaveMeshError> save_to_buffer(const SimpleMesh &mesh, const SaveOptions &options = {});

} // namespace mesh::io::terrain

// custom serialization
namespace zpp::bits {
template <typename T>
constexpr auto serialize(auto &archive, glm::tvec2<T> &v) {
    return archive(v.x, v.y);
}

template <typename T>
constexpr auto serialize(auto &archive, const glm::tvec2<T> &v) {
    return archive(v.x, v.y);
}

template <typename T>
constexpr auto serialize(auto &archive, glm::tvec3<T> &v) {
    return archive(v.x, v.y, v.z);
}

template <typename T>
constexpr auto serialize(auto &archive, const glm::tvec3<T> &v) {
    return archive(v.x, v.y, v.z);
}

auto serialize(auto &archive, cv::Mat &v) {
    std::vector<uint8_t> buf;
    auto result = archive(buf);
    v = mesh::io::texture::read_texture_from_encoded_bytes(buf);
    return result;
}

auto serialize(auto &archive, const cv::Mat &v) {
    return archive(mesh::io::texture::write_texture_to_encoded_buffer(v, ".png"));
}
} // namespace zpp::bits
