#include <algorithm>
#include <cstdint>
#include <queue>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/topology/adjacency.h"
#include "mesh/bfs.h"

namespace mesh {

BFS run_bfs(const uint32_t source, const std::span<const glm::uvec3> triangles) {
    return run_bfs(source, build_vertex_adjacency(triangles));
}
BFS run_bfs(const uint32_t source, std::vector<std::vector<uint32_t>> adjacency) {
    const uint32_t vertex_count = adjacency.size();

    BFS out;
    out.parent.assign(vertex_count, BFS::invalid);
    out.dist.assign(vertex_count, BFS::invalid);

    std::queue<uint32_t> queue;
    queue.push(source);
    out.parent[source] = source;
    out.dist[source] = 0;

    while (!queue.empty()) {
        const uint32_t current = queue.front();
        queue.pop();

        for (const uint32_t neighbour : adjacency[current]) {
            if (out.parent[neighbour] == BFS::invalid) {
                out.parent[neighbour] = current;
                out.dist[neighbour] = out.dist[current] + 1;
                queue.push(neighbour);
            }
        }
    }

    out.adjacency = std::move(adjacency);
    return out;
}

std::vector<uint32_t> reconstruct_path(const uint32_t start, const uint32_t end, const std::vector<uint32_t> &parent) {
    ASSERT(start < parent.size() && end < parent.size());
    if (parent[end] == BFS::invalid) {
        return {};
    }

    std::vector<uint32_t> path;
    for (uint32_t current = end; current != start; current = parent[current]) {
        DEBUG_ASSERT(current != BFS::invalid);
        path.push_back(current);
    }
    path.push_back(start);
    std::reverse(path.begin(), path.end());
    return path;
}

}
