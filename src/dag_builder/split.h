#pragma once

#include <array>
#include <cmath>
#include <type_traits>
#include <vector>

#include <libassert/assert.hpp>
#include <metis.h>

#include "Size.h"
#include "Buffer.h"
#include "clusterize.h"
#include "log.h"
#include "meshopt.h"
#include "number_types.h"
#include "utils.h"
#include "validate.h"

namespace {
template <typename A, typename B>
inline constexpr auto int_div_ceil(A a, B b) {
    static_assert(std::is_integral_v<A>);
    static_assert(std::is_integral_v<B>);
    using T = std::conditional_t<(sizeof(A) >= sizeof(B)), A, B>;
    ASSERT(b != 0);

    const T a_t = static_cast<T>(a);
    const T b_t = static_cast<T>(b);
    const T q = a_t / b_t;
    const T r = a_t % b_t;

    // Exact division -> already the ceiling
    if (r == 0) {
        return q;
    }

    // Same sign
    if ((a > 0 && b > 0) || (a < 0 && b < 0)) {
        return q + 1;
    }

    // Opposite signs -> truncation toward zero already gave the ceiling
    return q;
}
}

template <Size S>
inline Clustering split_each_into_equal_parts(const Clustering &input, const S num_parts) {
    if (num_parts == 0) {
        return {
            .positions = input.positions,
            .clusters = {},
            .textures = input.textures
        };
    }
    if (num_parts == 1) {
        return input;
    }

    validate(input);
    std::vector<Cluster> new_clusters;
    for (const auto& cluster : input.clusters) {
        const uint32_t triangle_count = cluster.local_triangles.size();
        const uint32_t vertex_count = cluster.vertex_indices.size();
        const bool has_uvs = !cluster.uvs.empty();

        // Ensure we can meaningfully split triangles and vertices
        ASSERT(triangle_count >= num_parts && "Cluster has fewer triangles than requested parts");
        ASSERT(vertex_count >= num_parts && "Cluster has fewer vertices than requested parts");

        // Initialize eptr and eind based on the triangle data<
        std::vector<idx_t> eptr(triangle_count + 1); // Offsets in eind where faces start/end
        std::vector<idx_t> eind(triangle_count * 3); // Indices into vertex array

        eptr[0] = 0;
        for (uint32_t i = 0; i < triangle_count; i++) {
            eptr[i + 1] = eptr[i] + 3;

            const glm::uvec3 triangle = cluster.local_triangles[i];
            eind[i * 3] = triangle.x;
            eind[i * 3 + 1] = triangle.y;
            eind[i * 3 + 2] = triangle.z;
        }

        // Initialize options to default values
        std::array<idx_t, METIS_NOPTIONS> options = {};
        METIS_SetDefaultOptions(options.data());
        options[METIS_OPTION_NUMBERING] = 0;

        // Allocate memory for output vectors
        std::vector<idx_t> epart(triangle_count); // Map triangle index -> partition id
        std::vector<idx_t> npart(vertex_count); // Map vertex index -> partition id

        // Perform partitioning
        idx_t quality;
        idx_t num_parts_mut = num_parts;
        idx_t triangle_count_mut = triangle_count;
        idx_t vertex_count_mut = vertex_count;
        // TODO: Try METIS_PartGraphKway as it supports edges weights which could be set to their lengths
        auto result = METIS_PartMeshNodal(
            &triangle_count_mut,
            &vertex_count_mut,
            eptr.data(),
            eind.data(),
            nullptr,
            nullptr,
            &num_parts_mut,
            nullptr,
            options.data(),
            &quality,
            epart.data(),
            npart.data());
        ASSERT(METIS_OK == result);

        // Allocate new clusters
        Buffer<Cluster, S> partitions = make_buffer<Cluster>(num_parts);
        const size_t expected_part_vertex_count = int_div_ceil(vertex_count, num_parts.value());
        const size_t expected_part_triangle_count = int_div_ceil(triangle_count, num_parts.value());
        for (Cluster &partition : partitions) {
            partition.texture_id = cluster.texture_id;
            partition.vertex_indices.reserve(expected_part_vertex_count * 3 / 2);
            partition.local_triangles.reserve(expected_part_triangle_count * 3 / 2);
            if (has_uvs) {
                partition.uvs.reserve(expected_part_vertex_count * 3 / 2);
            }
        }
        Buffer<std::vector<uint32_t>, S> remap = make_buffer<std::vector<uint32_t>>(num_parts);
        const uint32_t invalid_remap = -1;
        for (std::vector<uint32_t>& remap_of_part : remap) {
            remap_of_part.resize(vertex_count, invalid_remap);
        }

        // Convert METIS output to cluster vertices and triangles
        // Note that we cant use npart since it only assigns border vertices to one partition
        for (uint32_t i = 0; i < triangle_count; i++) {
            const uint32_t partition_index = epart[i];
            Cluster &partition = partitions[partition_index];
            glm::uvec3 triangle = cluster.local_triangles[i];
            for (uint8_t k=0; k<3; k++) {
                const uint32_t original_index = triangle[k];
                uint32_t &new_index = remap[partition_index][original_index];
                if (new_index == invalid_remap) {
                    new_index = partition.vertex_indices.size();
                    partition.vertex_indices.push_back(cluster.vertex_indices[original_index]);
                    if (has_uvs) {
                        partition.uvs.push_back(cluster.uvs[original_index]);
                    }
                }
                triangle[k] = new_index;
            }
            partition.local_triangles.push_back(triangle);
        }

        // Validate
        for (const Cluster &partition : partitions) {
            validate(partition, input.positions);
        }

        new_clusters.insert(
            new_clusters.end(),
            std::make_move_iterator(partitions.begin()),
            std::make_move_iterator(partitions.end()));
    }

    return Clustering{
        input.positions,
        std::move(new_clusters),
        input.textures
    };
}

template <size_t NUM_PARTS>
inline Clustering split_each_into_equal_parts(const Clustering &input) {
    return split_each_into_equal_parts(input, make_size<NUM_PARTS>());
}
inline Clustering split_each_into_equal_parts(const Clustering &input, const size_t num_parts = 2) {
    return split_each_into_equal_parts(input, make_size(num_parts));
}
