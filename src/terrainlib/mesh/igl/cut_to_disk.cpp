#include <span>
#include <vector>
#include <ranges>
#include <algorithm>

#include <Eigen/Core>
#include <glm/common.hpp>
#include <igl/cut_to_disk.h>
#include <libassert/assert.hpp>

#include "mesh/igl/convert.h"
#include "mesh/igl/cut_to_disk.h"
#include "mesh/connectivity/boundary.h"
#include "mesh/bfs.h"
#include "mesh/connectivity/topology.h"
#include "mesh/connectivity/connected_components.h"
#include "mesh/reindex.h"
#include "vector_utils.h"

namespace mesh {

namespace detail {
uint32_t find_farthest_vertex(const BFS& bfs) {
    const auto it = std::max_element(bfs.dist.begin(), bfs.dist.end());
    return std::distance(bfs.dist.begin(), it);
}

std::vector<uint32_t> find_sphere_cut(
    const std::span<const glm::uvec3> &triangles) {
    if (triangles.empty()) {
        return {};
    }
    const uint32_t start = triangles[0][0];
    const BFS bfs = run_bfs(start, triangles);
    const uint32_t farthest = find_farthest_vertex(bfs);
    std::vector<uint32_t> path = reconstruct_path(start, farthest, bfs.parent);
    // Since cut_mesh ignores cuts consisting of a single edge, we need to ensure that the cut has at least 3 vertices
    DEBUG_ASSERT(path.size() >= 2);
    if (path.size() == 2) {
        for (const uint32_t vertex : path) {
            for (const uint32_t neighbor : bfs.adjacency[vertex]) {
                if (!contains(path, neighbor)) {
                    // found a vertex not in the path -> add it to the path
                    path.push_back(neighbor);
                    break;
                }
            }
            if (path.size() == 3) {
                break;
            }
        }
    }
    return path;
}

void find_cut_to_disk_for_sphere(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts) {
    const auto [reindexed, index_map] = reindex_with_map(triangles);

    if (!compute_topology(reindexed).is_sphere()) {
        return;
    }

    auto cut = find_sphere_cut(reindexed);
    for (auto& vertex_index : cut) {
        vertex_index = index_map.map_backward(vertex_index);
    }
    cuts.push_back(cut);
}

void find_cut_to_disk_igl(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts) {
    const auto F = convert_triangles(triangles);
    igl::cut_to_disk(F, cuts);
}

void remove_boundary_from_cuts(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts) {
    const auto boundary_edges = find_boundary_edges(triangles);

    std::vector<uint32_t> cuts_to_remove;
    std::vector<std::vector<uint32_t>> cuts_to_add;

    for (auto& [i, cut] : enumerate(cuts)) {
        // Remove empty cuts
        if (cut.size() < 2) {
            cuts_to_remove.push_back(i);
            continue;
        }

        // Iterate over cut and remove boundary segments
        for (uint32_t j = cut.size() - 1; j > 0; j--) {
            const uint32_t current_vertex = cut[j];
            const uint32_t next_vertex = cut[j - 1];
            const glm::uvec2 edge(next_vertex, current_vertex);
            const bool is_boundary = boundary_edges.contains(edge);
            if (is_boundary) {
                // found boundary edge
                if (cut.size() - j > 1) {
                    // not at the end -> copy segment
                    std::vector<uint32_t> new_cut(cut.begin() + j, cut.end());
                    cuts_to_add.push_back(new_cut);
                }

                // remove boundary edge
                cut.resize(j);
            }
        }

        if (cut.size() < 2) {
            cuts_to_remove.push_back(i);
        }
    }

    // Remove boundary cuts
    for (const uint32_t cut_index : std::views::reverse(cuts_to_remove)) {
        erase_by_index(cuts, cut_index);
    }

    // Add partial cuts
    cuts.insert(cuts.end(), cuts_to_add.begin(), cuts_to_add.end());
}
} // namespace detail

void find_cut_to_disk(const std::span<const glm::uvec3> &triangles, std::vector<std::vector<uint32_t>> &cuts) {
    detail::find_cut_to_disk_igl(triangles, cuts);
    detail::remove_boundary_from_cuts(triangles, cuts);

    std::vector<std::vector<glm::uvec3>> triangles_per_component = split_into_connected_components(triangles);
    for (const auto& component : triangles_per_component) {
        detail::find_cut_to_disk_for_sphere(component, cuts);
    }
}
}
