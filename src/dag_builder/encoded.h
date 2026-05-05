#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <type_traits>
#include <vector>

#include <tl/expected.hpp>
#include <zpp_bits.h>
#include <glm/glm.hpp>
#include <opencv2/core.hpp>

#include "cluster.h"
#include "io/bytes.h"
#include "io/serialize.h"
#include "mesh/io/texture.h"
#include "meshopt.h"

namespace zpp::bits {

template <typename Archive>
auto serialize(Archive &archive, const cv::Mat &image) {
    std::vector<uint8_t> encoded = mesh::io::write_texture_to_encoded_buffer(image, ".png");
    return archive(encoded);
}

template <typename Archive>
auto serialize(Archive &archive, cv::Mat &image) {
    std::vector<uint8_t> encoded;

    auto result = archive(encoded);
    if (failure(result)) {
        return result;
    }

    image = mesh::io::read_texture_from_encoded_bytes(encoded);
    return result;
}

template <typename Archive>
auto serialize(Archive &archive, const Cluster &cluster) {
    const size_t triangle_count = cluster.triangle_count();
    const size_t vertex_count = cluster.vertex_count();
    const size_t uv_count = cluster.uvs.size();

    auto encoded_triangles = meshopt::encode_index_buffer(cluster.local_triangles);
    auto encoded_vertex_indices = meshopt::encode_vertex_buffer(cluster.vertex_indices);
    auto encoded_uvs = meshopt::encode_vertex_buffer(cluster.uvs);

    return archive(
        triangle_count,
        vertex_count,
        uv_count,
        encoded_triangles,
        encoded_vertex_indices,
        encoded_uvs,
        cluster.texture_id,
        cluster.absolute_error);
}
template <typename Archive>
auto serialize(Archive &archive, Cluster &cluster) {
    size_t triangle_count;
    size_t vertex_count;
    size_t uv_count;

    std::vector<uint8_t> encoded_triangles;
    std::vector<uint8_t> encoded_vertex_indices;
    std::vector<uint8_t> encoded_uvs;

    auto result = archive(
        triangle_count,
        vertex_count,
        uv_count,
        encoded_triangles,
        encoded_vertex_indices,
        encoded_uvs,
        cluster.texture_id,
        cluster.absolute_error);

    if (failure(result)) {
        return result;
    }

    meshopt::decode_index_buffer(cluster.local_triangles, triangle_count, encoded_triangles);
    meshopt::decode_vertex_buffer(cluster.vertex_indices, vertex_count, encoded_vertex_indices);
    meshopt::decode_vertex_buffer(cluster.uvs, uv_count, encoded_uvs);

    return result;
}

template <typename Archive>
auto serialize(Archive &archive, const Clustering &clustering) {
    const size_t vertex_count = clustering.vertex_count();
    auto encoded_positions = meshopt::encode_vertex_buffer(clustering.positions);

    return archive(
        vertex_count,
        encoded_positions,
        clustering.clusters,
        clustering.textures);
}

template <typename Archive>
auto serialize(Archive &archive, Clustering &clustering) {
    size_t vertex_count;
    std::vector<uint8_t> encoded_positions;

    auto result = archive(
        vertex_count,
        encoded_positions,
        clustering.clusters,
        clustering.textures);

    if (failure(result)) {
        return result;
    }

    meshopt::decode_vertex_buffer(clustering.positions, vertex_count, encoded_positions);

    return result;
}

} // namespace zpp::bits

inline tl::expected<void, ::io::Error>
save_clustering(const Clustering &clustering,
                 const std::filesystem::path &path,
                 const bool make_dirs = true) {
    return ::io::write_to_path(clustering, path, make_dirs);
}

inline tl::expected<Clustering, ::io::Error>
load_clustering(const std::filesystem::path &path) {
    return ::io::read_from_path<Clustering>(path);
}
