/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <cstdint>
#include <span>
#include <vector>
#include <numeric>

#include <glm/common.hpp>

#include "build_config.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/igl/cut.h"
#include "enumerate.h"
#include "mesh/connectivity/vertex_index_range.h"
#include "mesh/connectivity/topology.h"

#include "mesh/connectivity/connected_components.h"

namespace mesh {

inline std::vector<std::vector<uint32_t>> find_cut_to_disk(const std::span<const glm::uvec3> &triangles) {
    std::vector<std::vector<uint32_t>> cuts;
    find_cut_to_disk(triangles, cuts);
    return cuts;
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::Simple_<n_dims, T> &mesh) {
    return find_cut_to_disk(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_cut_to_disk(const mesh::View_<n_dims, T> &mesh) {
    return find_cut_to_disk(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(mesh::Simple_<n_dims, T> &mesh) {
    return cut_to_disk(mesh.triangles, mesh.positions, mesh.uvs);
}

namespace detail {
inline std::vector<uint32_t> identity_map(const uint32_t vertex_count) {
    std::vector<uint32_t> map(vertex_count);
    std::iota(map.begin(), map.end(), 0);
    return map;
}
inline std::vector<uint32_t> identity_map(const std::span<const glm::uvec3> triangles) {
    const uint32_t max_vertex_index = find_max_vertex_index(triangles);
    return identity_map(max_vertex_index + 1);
}
inline void assert_disks(const std::span<const glm::uvec3> triangles) {
#ifndef NDEBUG
    const Topology topology = compute_topology(triangles);
    for (const auto &component : topology.components()) {
        DEBUG_ASSERT(component.is_disk(true));
    }
    DEBUG_ASSERT(topology.is_disks(true));
#else
    ALP_UNUSED(triangles);
#endif
}
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::span<const glm::vec<n_dims, T>> positions) {
    const auto cuts = find_cut_to_disk(triangles);
    if (cuts.empty()) {
        return detail::identity_map(positions.size());
    }
    const auto cut_edge_mask = cuts_to_edge_mask(cuts, triangles);
    const auto map = cut(triangles, positions, cut_edge_mask);
    detail::assert_disks(triangles);
    return map;
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions) {
    const auto cuts = find_cut_to_disk(triangles);
    if (cuts.empty()) {
        return detail::identity_map(positions.size());
    }
    const auto cut_edge_mask = cuts_to_edge_mask(cuts, triangles);
    const auto map = cut(triangles, positions, cut_edge_mask);
    detail::assert_disks(triangles);
    return map;
}

template <glm::length_t n_dims, typename T>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    std::vector<glm::vec<n_dims, T>> &positions,
    std::vector<glm::vec<2, T>> &uvs) {
    const auto cuts = find_cut_to_disk(triangles);
    if (cuts.empty()) {
        return detail::identity_map(positions.size());
    }
    const auto cut_edge_mask = cuts_to_edge_mask(cuts, triangles);
    const auto map = cut(triangles, positions, uvs, cut_edge_mask);
    detail::assert_disks(triangles);
    return map;
}

inline std::vector<uint32_t> cut_to_disk(std::span<glm::uvec3> triangles) {
    const auto cuts = find_cut_to_disk(triangles);
    if (cuts.empty()) {
        return detail::identity_map(triangles);
    }
    const auto cut_edge_mask = cuts_to_edge_mask(cuts, triangles);
    const auto map = cut(triangles, cut_edge_mask);
    detail::assert_disks(triangles);
    return map;
}

template <typename Reserve, typename Duplicate>
std::vector<uint32_t> cut_to_disk(
    std::span<glm::uvec3> triangles,
    Reserve &&reserve,
    Duplicate &&duplicate) {
    const auto cuts = find_cut_to_disk(triangles);
    if (cuts.empty()) {
        return detail::identity_map(triangles);
    }
    const auto cut_edge_mask = cuts_to_edge_mask(cuts, triangles);
    const auto map = cut(
        triangles,
        cut_edge_mask,
        std::forward<Reserve>(reserve),
        std::forward<Duplicate>(duplicate));
    detail::assert_disks(triangles);
    return map;
}
}
