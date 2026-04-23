#include <cstdint>
#include <span>
#include <vector>
#include <ranges>

#include <glm/common.hpp>

#include "enumerate.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/normalize.h"
#include "mesh/vertex_index_range.h"

namespace mesh {

template <std::ranges::range Cuts>
std::vector<glm::bvec3> cuts_to_edge_mask(
    const Cuts cuts,
    const std::span<const glm::uvec3> triangles) {
    std::unordered_set<glm::uvec2> cut_edges;
    for (const auto &cut : cuts) {
        if (cut.size() < 2) {
            continue;
        }

        for (size_t i = 0; i + 1 < cut.size(); i++) {
            cut_edges.insert(mesh::normalize_edge(glm::uvec2(cut[i], cut[i + 1])));
        }
    }

    std::vector<glm::bvec3> edge_cut_mask(triangles.size(), glm::bvec3(false));
    for (const auto &[i, triangle] : enumerate(triangles)) {
        for (uint8_t k = 0; k < 3; k++) {
            const uint32_t a = triangle[k];
            const uint32_t b = triangle[(k + 1) % 3];
            const glm::uvec2 edge = mesh::normalize_edge(glm::uvec2(a, b));
            edge_cut_mask[i][k] = cut_edges.contains(edge);
        }
    }

    return edge_cut_mask;
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    mesh::Simple_<n_dims, T> &mesh,
    const std::span<const glm::bvec3> edge_cut_mask) {
    return cut(mesh.triangles, mesh.positions, mesh.uvs, edge_cut_mask);
}
template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    std::vector<glm::vec<2, T>> &uvs,
    const std::span<const glm::bvec3> edge_cut_mask) {
    if (uvs.empty()) {
        return cut(triangles, positions, edge_cut_mask);
    }
    DEBUG_ASSERT(positions.size() == uvs.size());
    auto reserve = [&](const uint32_t new_vertex_count) {
        DEBUG_ASSERT(new_vertex_count >= positions.size());
        positions.resize(new_vertex_count);
        uvs.resize(new_vertex_count);
    };
    auto duplicate = [&](const uint32_t old_index, const uint32_t new_index) {
        positions[new_index] = positions[old_index];
        uvs[new_index] = uvs[old_index];
    };
    return cut(triangles, edge_cut_mask, reserve, duplicate);
}
template <typename Reserve, typename Duplicate>
std::vector<uint32_t> cut(
    std::span<glm::uvec3> triangles,
    const std::span<const glm::bvec3> edge_cut_mask,
    Reserve &&reserve,
    Duplicate &&duplicate) {
    const uint32_t old_vertex_count = mesh::compute_vertex_count(triangles);

    const std::vector<uint32_t> mapping = cut(triangles, edge_cut_mask);
    const uint32_t new_vertex_count = mapping.size();
    if (new_vertex_count == old_vertex_count) {
        return mapping;
    }

    DEBUG_ASSERT(new_vertex_count > old_vertex_count);
    reserve(new_vertex_count);

    std::vector<bool> encountered(old_vertex_count, false);
    for (const auto [new_vertex_index, old_vertex_index] : enumerate(mapping)) {
        if (!encountered[old_vertex_index]) {
            encountered[old_vertex_index] = true;
            continue;
        }

        duplicate(old_vertex_index, new_vertex_index);
    }

    return mapping;
}

} // namespace mesh
