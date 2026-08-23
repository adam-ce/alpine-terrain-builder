#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <expected>
#include <meshoptimizer.h>
#include <opencv2/core.hpp>

#include "cluster.h"
#include "dag_node.h"
#include "io/envelope.h"
#include "mesh/io/texture.h"
#include "meshopt.h"

namespace dag::format {

namespace v1 {

    struct OctreeId {
        std::uint8_t level;
        std::uint64_t index;
    };

    struct Id {
        OctreeId source_batch;
        std::uint32_t cluster_index;
    };

    struct Vec3d {
        double x;
        double y;
        double z;
    };

    struct Bounds {
        Vec3d min;
        Vec3d max;
    };

    struct Group {
        std::vector<Id> children;
        double error;
        Bounds bounds;
    };

    struct NodeMetadata {
        std::vector<std::uint32_t> group_assignment;
        std::vector<Group> groups;
    };

    struct Texture {
        std::vector<std::uint8_t> jpeg;
    };

    struct Cluster {
        std::uint64_t triangle_count;
        std::uint64_t vertex_count;
        std::uint64_t uv_count;
        std::vector<std::uint8_t> encoded_triangles;
        std::vector<std::uint8_t> encoded_vertex_indices;
        std::vector<std::uint8_t> encoded_uvs;
        std::uint32_t id;
        std::uint32_t texture_id;
        double absolute_error;
    };

    struct Clustering {
        std::uint64_t vertex_count;
        std::vector<std::uint8_t> encoded_positions;
        std::vector<Cluster> clusters;
        std::vector<Texture> textures;
    };

} // namespace v1

using MetadataSchema = io::envelope::PayloadSchema<"dag.NodeMetadata", io::envelope::Version<1, v1::NodeMetadata>>;
using ClusteringSchema = io::envelope::PayloadSchema<"dag.Clustering", io::envelope::Version<1, v1::Clustering>>;
using NodeMetadata = MetadataSchema::latest_type;
using Clustering = ClusteringSchema::latest_type;

inline v1::OctreeId encode_id(const octree::Id& id) { return { .level = id.level(), .index = id.index_on_level() }; }

inline std::expected<octree::Id, std::string> decode_id(const v1::OctreeId& id)
{
    auto result = octree::Id::try_make(static_cast<octree::Id::Level>(id.level), static_cast<octree::Id::Index>(id.index));
    if (!result) {
        return std::unexpected("DAG payload contains an invalid octree ID");
    }
    return *result;
}

inline NodeMetadata encode_metadata(const dag::NodeMetadata& metadata)
{
    NodeMetadata result;
    result.group_assignment = metadata.group_assignment;
    result.groups.reserve(metadata.groups.size());
    for (const dag::Group& group : metadata.groups) {
        v1::Group encoded_group{
            .children = {},
            .error = group.error,
            .bounds = {
                .min = {group.bounds.min.x, group.bounds.min.y, group.bounds.min.z},
                .max = {group.bounds.max.x, group.bounds.max.y, group.bounds.max.z},
            },
        };
        encoded_group.children.reserve(group.children.size());
        for (const dag::Id& child : group.children) {
            encoded_group.children.push_back({
                .source_batch = encode_id(child.source_batch),
                .cluster_index = child.cluster_index,
            });
        }
        result.groups.push_back(std::move(encoded_group));
    }
    return result;
}

inline std::expected<void, std::string> validate(const NodeMetadata& metadata)
{
    for (const std::uint32_t group : metadata.group_assignment) {
        if (group >= metadata.groups.size()) {
            return std::unexpected("DAG metadata contains an invalid group assignment");
        }
    }
    for (const v1::Group& group : metadata.groups) {
        if (!std::isfinite(group.error)) {
            return std::unexpected("DAG metadata contains a non-finite error");
        }
        for (const v1::Id& child : group.children) {
            if (!decode_id(child.source_batch)) {
                return std::unexpected("DAG metadata contains an invalid child ID");
            }
        }
    }
    return {};
}

inline std::expected<dag::NodeMetadata, std::string> decode_metadata(NodeMetadata metadata)
{
    if (auto valid = validate(metadata); !valid) {
        return std::unexpected(valid.error());
    }
    dag::NodeMetadata result;
    result.group_assignment = std::move(metadata.group_assignment);
    result.groups.reserve(metadata.groups.size());
    for (v1::Group& group : metadata.groups) {
        dag::Group decoded_group{
            .children = {},
            .error = group.error,
            .bounds = {
                {group.bounds.min.x, group.bounds.min.y, group.bounds.min.z},
                {group.bounds.max.x, group.bounds.max.y, group.bounds.max.z},
            },
        };
        decoded_group.children.reserve(group.children.size());
        for (const v1::Id& child : group.children) {
            auto source_batch = decode_id(child.source_batch);
            if (!source_batch) {
                return std::unexpected(source_batch.error());
            }
            decoded_group.children.push_back({ *source_batch, child.cluster_index });
        }
        result.groups.push_back(std::move(decoded_group));
    }
    return result;
}

inline std::expected<Clustering, std::string> encode_clustering(const ::Clustering& clustering)
{
    Clustering result {
        .vertex_count = clustering.positions.size(),
        .encoded_positions = meshopt::encode_vertex_buffer(clustering.positions),
        .clusters = {},
        .textures = {},
    };
    result.clusters.reserve(clustering.clusters.size());
    for (const ::Cluster& cluster : clustering.clusters) {
        if (cluster.texture_id == (std::numeric_limits<std::uint32_t>::max)()) {
            return std::unexpected("DAG texture ID collides with the on-disk null sentinel");
        }
        result.clusters.push_back({
            .triangle_count = cluster.local_triangles.size(),
            .vertex_count = cluster.vertex_indices.size(),
            .uv_count = cluster.uvs.size(),
            .encoded_triangles = meshopt::encode_index_buffer(cluster.local_triangles),
            .encoded_vertex_indices = meshopt::encode_vertex_buffer(cluster.vertex_indices),
            .encoded_uvs = meshopt::encode_vertex_buffer(cluster.uvs),
            .id = cluster.id,
            .texture_id = cluster.texture_id.value_or((std::numeric_limits<std::uint32_t>::max)()),
            .absolute_error = cluster.absolute_error,
        });
    }
    result.textures.reserve(clustering.textures.size());
    try {
        for (const cv::Mat& texture : clustering.textures) {
            auto jpeg = mesh::io::write_texture_to_encoded_buffer(texture, ".jpeg");
            if (!texture.empty() && jpeg.empty()) {
                return std::unexpected("could not encode DAG texture");
            }
            result.textures.push_back({ std::move(jpeg) });
        }
    } catch (const cv::Exception&) {
        return std::unexpected("could not encode DAG texture");
    }
    return result;
}

inline bool count_fits(const std::uint64_t count, const std::size_t element_size)
{
    return count <= std::numeric_limits<std::size_t>::max() / element_size && count * element_size <= io::envelope::default_max_decompressed_size;
}

inline std::expected<void, std::string> validate(const Clustering& clustering)
{
    if (!count_fits(clustering.vertex_count, sizeof(glm::dvec3))) {
        return std::unexpected("DAG position count exceeds the allocation limit");
    }
    if (clustering.vertex_count != 0 && clustering.encoded_positions.empty()) {
        return std::unexpected("DAG position count and encoded data disagree");
    }
    std::size_t decoded_size = static_cast<std::size_t>(clustering.vertex_count) * sizeof(glm::dvec3);
    for (const v1::Cluster& cluster : clustering.clusters) {
        if (!count_fits(cluster.triangle_count, sizeof(glm::uvec3)) || !count_fits(cluster.vertex_count, sizeof(std::uint32_t))
            || !count_fits(cluster.uv_count, sizeof(glm::dvec2))) {
            return std::unexpected("DAG cluster count exceeds the allocation limit");
        }
        const std::size_t cluster_size = static_cast<std::size_t>(cluster.triangle_count) * sizeof(glm::uvec3)
            + static_cast<std::size_t>(cluster.vertex_count) * sizeof(std::uint32_t) + static_cast<std::size_t>(cluster.uv_count) * sizeof(glm::dvec2);
        if (cluster_size > io::envelope::default_max_decompressed_size - decoded_size) {
            return std::unexpected("DAG decoded data exceeds the allocation limit");
        }
        decoded_size += cluster_size;
        if (cluster.uv_count != 0 && cluster.uv_count != cluster.vertex_count) {
            return std::unexpected("DAG cluster UV count does not match its vertex count");
        }
        if ((cluster.triangle_count != 0 && cluster.encoded_triangles.empty()) || (cluster.vertex_count != 0 && cluster.encoded_vertex_indices.empty())
            || (cluster.uv_count != 0 && cluster.encoded_uvs.empty())) {
            return std::unexpected("DAG cluster count and encoded data disagree");
        }
        if (cluster.texture_id != (std::numeric_limits<std::uint32_t>::max)() && cluster.texture_id >= clustering.textures.size()) {
            return std::unexpected("DAG cluster contains an invalid texture reference");
        }
        if (!std::isfinite(cluster.absolute_error)) {
            return std::unexpected("DAG cluster contains a non-finite error");
        }
    }
    return {};
}

template <typename T>
inline std::expected<std::vector<T>, std::string> decode_vertex_buffer(
    const std::uint64_t count, const std::vector<std::uint8_t>& encoded, const std::string_view label)
{
    if (!count_fits(count, sizeof(T))) {
        return std::unexpected(std::string(label) + " count exceeds the allocation limit");
    }
    std::vector<T> result(static_cast<std::size_t>(count));
    if (meshopt_decodeVertexBuffer(result.data(), result.size(), sizeof(T), encoded.data(), encoded.size()) != 0) {
        return std::unexpected("could not decode " + std::string(label));
    }
    return result;
}

inline std::expected<std::vector<glm::uvec3>, std::string> decode_triangles(const std::uint64_t count, const std::vector<std::uint8_t>& encoded)
{
    if (!count_fits(count, sizeof(glm::uvec3))) {
        return std::unexpected("DAG triangle count exceeds the allocation limit");
    }
    std::vector<glm::uvec3> result(static_cast<std::size_t>(count));
    if (meshopt_decodeIndexBuffer(result.data(), result.size() * 3, sizeof(std::uint32_t), encoded.data(), encoded.size()) != 0) {
        return std::unexpected("could not decode DAG triangles");
    }
    return result;
}

inline std::expected<::Clustering, std::string> decode_clustering(Clustering clustering)
{
    if (auto valid = validate(clustering); !valid) {
        return std::unexpected(valid.error());
    }
    auto positions = decode_vertex_buffer<glm::dvec3>(clustering.vertex_count, clustering.encoded_positions, "DAG positions");
    if (!positions) {
        return std::unexpected(positions.error());
    }

    ::Clustering result;
    result.positions = std::move(*positions);
    result.clusters.reserve(clustering.clusters.size());
    for (const v1::Cluster& encoded : clustering.clusters) {
        auto triangles = decode_triangles(encoded.triangle_count, encoded.encoded_triangles);
        auto vertex_indices = decode_vertex_buffer<std::uint32_t>(encoded.vertex_count, encoded.encoded_vertex_indices, "DAG vertex indices");
        auto uvs = decode_vertex_buffer<glm::dvec2>(encoded.uv_count, encoded.encoded_uvs, "DAG UVs");
        if (!triangles) {
            return std::unexpected(triangles.error());
        }
        if (!vertex_indices) {
            return std::unexpected(vertex_indices.error());
        }
        if (!uvs) {
            return std::unexpected(uvs.error());
        }
        for (const std::uint32_t vertex : *vertex_indices) {
            if (vertex >= result.positions.size()) {
                return std::unexpected("DAG cluster contains an invalid global vertex index");
            }
        }
        for (const glm::uvec3 triangle : *triangles) {
            if (glm::any(glm::greaterThanEqual(triangle, glm::uvec3(static_cast<std::uint32_t>(vertex_indices->size()))))) {
                return std::unexpected("DAG cluster contains an invalid local triangle index");
            }
        }
        result.clusters.push_back({
            .id = encoded.id,
            .vertex_indices = std::move(*vertex_indices),
            .local_triangles = std::move(*triangles),
            .uvs = std::move(*uvs),
            .texture_id
            = encoded.texture_id == (std::numeric_limits<std::uint32_t>::max)() ? std::nullopt : std::optional<std::uint32_t> { encoded.texture_id },
            .absolute_error = encoded.absolute_error,
        });
    }

    std::vector<cv::Mat> textures;
    textures.reserve(clustering.textures.size());
    try {
        for (const v1::Texture& texture : clustering.textures) {
            cv::Mat image = mesh::io::read_texture_from_encoded_bytes(texture.jpeg);
            if (image.empty()) {
                return std::unexpected("could not decode DAG texture");
            }
            textures.push_back(std::move(image));
        }
    } catch (const cv::Exception&) {
        return std::unexpected("could not decode DAG texture");
    }
    result.textures = TextureSet(std::move(textures));
    return result;
}

} // namespace dag::format
