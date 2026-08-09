#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Range.h"
#include "build_config.h"
#include "glm_utils.h"
#include "OffsetVector.h"
#include "mesh/SimpleMesh.h"
#include "mesh/VertexMap.h"
#include "mesh/View.h"
#include "mesh/reindex.h"
#include "mesh/topology/vertex_index_range.h"

namespace mesh {

namespace detail {
constexpr uint32_t invalid_index = VertexMap::invalid_index;
} // namespace detail

inline VertexMap create_reindex_map(const std::span<const glm::uvec3> triangles) {
    const Range<uint32_t> vertex_range = find_vertex_index_range(triangles);
    OffsetVector<uint32_t> old_to_new;
    old_to_new.offset = vertex_range.start;
    old_to_new.resize(vertex_range.size(), detail::invalid_index);

    uint32_t next_index = 0;
    for (const glm::uvec3 &triangle : triangles) {
        for (uint8_t k = 0; k < 3; k++) {
            const uint32_t old_index = triangle[k];
            if (old_to_new[old_index] != detail::invalid_index) {
                continue;
            }

            old_to_new[old_index] = next_index;
            next_index++;
        }
    }

    return VertexMap::from_forward(old_to_new);
}


inline TrianglesAndMap reindex_with_map(const std::span<const glm::uvec3> triangles) {
    const VertexMap remap = create_reindex_map(triangles);

    std::vector<glm::uvec3> new_triangles;
    new_triangles.reserve(triangles.size());
    for (const glm::uvec3 &triangle : triangles) {
        const glm::uvec3 new_triangle = remap.map_triangle_forward(triangle);
        new_triangles.push_back(new_triangle);
    }
    return {std::move(new_triangles), remap};
}
inline std::vector<glm::uvec3> reindex(std::span<const glm::uvec3> triangles) {
    return reindex_with_map(triangles).triangles;
}

template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::View_<n_dims, T> &mesh) {
    const auto [new_triangles, remap] = reindex_with_map(mesh.triangles);

    std::vector<glm::vec<n_dims, T>> new_positions;
    new_positions.reserve(remap.new_vertex_count());
    for (const uint32_t old_index : remap.backward()) {
        new_positions.push_back(mesh.positions[old_index]);
    }

    std::vector<glm::vec<2, T>> new_uvs;
    if (mesh.has_uvs()) {
        new_uvs.reserve(remap.new_vertex_count());
        for (const uint32_t old_index : remap.backward()) {
            new_uvs.push_back(mesh.uvs[old_index]);
        }
    }

    mesh::Simple_<n_dims, T> new_mesh(new_triangles, new_positions, new_uvs, mesh.texture);
    return {std::move(new_mesh), remap};
}
template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::View_<n_dims, T> &mesh) {
    return reindex_with_map(mesh).mesh;
}

template <glm::length_t n_dims, typename T>
MeshAndMap<n_dims, T> reindex_with_map(const mesh::Simple_<n_dims, T> &mesh) {
    return reindex_with_map(mesh::View_<n_dims, T>(mesh));
}
template <glm::length_t n_dims, typename T>
mesh::Simple_<n_dims, T> reindex(const mesh::Simple_<n_dims, T> &mesh) {
    return reindex_with_map(mesh).mesh;
}

namespace detail {
inline void reindex_inplace(const std::span<glm::uvec3> triangles, const VertexMap &remap) {
    for (glm::uvec3 &triangle : triangles) {
        triangle = remap.map_triangle_forward(triangle);
    }
}
}
inline VertexMap reindex_inplace_with_map(const std::span<glm::uvec3> triangles) {
    const VertexMap remap = create_reindex_map(triangles);
    detail::reindex_inplace(triangles, remap);
    return remap;
}
inline void reindex_inplace(std::span<glm::uvec3> triangles) {
    reindex_inplace_with_map(triangles);
}


namespace detail {

template <typename... Ts>
void permute_inplace(
    OffsetVector<uint32_t> &old_to_new,
    const std::span<uint32_t> new_to_old,
    std::vector<Ts> &...values) {
    const uint32_t new_vertex_count = new_to_old.size();
    const uint32_t old_offset = old_to_new.offset;

    for (uint32_t new_index = 0; new_index < new_vertex_count; new_index++) {
        const uint32_t old_index = new_to_old[new_index];
        DEBUG_ASSERT(old_index != detail::invalid_index);
        (std::swap(values[old_index], values[new_index]), ...);

        if (new_index < old_offset) {
            continue;
        }

        const uint32_t prev_at_new = old_to_new[new_index];
        if (prev_at_new == detail::invalid_index) {
            if constexpr (IS_DEBUG_BUILD) {
                new_to_old[new_index] = new_index;
            }
        } else {
            std::swap(new_to_old[new_index], new_to_old[prev_at_new]);
        }
        std::swap(old_to_new[old_index], old_to_new[new_index]);
    }

    (values.resize(new_vertex_count), ...);
}

template <glm::length_t n_dims, typename T>
void reindex_inplace(mesh::Simple_<n_dims, T> &mesh, VertexMap remap) {
    detail::reindex_inplace(mesh.triangles, remap);
    DEBUG_ASSERT(remap.backward().offset == 0);

    if (mesh.has_uvs()) {
        detail::permute_inplace(
            remap.forward(),
            remap.backward().data,
            mesh.positions,
            mesh.uvs);
    } else {
        detail::permute_inplace(
            remap.forward(),
            remap.backward().data,
            mesh.positions);
    }
}
}

template <glm::length_t n_dims, typename T>
VertexMap reindex_inplace_with_map(mesh::Simple_<n_dims, T> &mesh) {
    const VertexMap remap = create_reindex_map(mesh.triangles);
    detail::reindex_inplace(mesh, remap);
    return remap;
}

template <glm::length_t n_dims, typename T>
inline void reindex_inplace(mesh::Simple_<n_dims, T> &mesh) {
    VertexMap remap = create_reindex_map(mesh.triangles);
    detail::reindex_inplace(mesh, std::move(remap));
}

} // namespace mesh