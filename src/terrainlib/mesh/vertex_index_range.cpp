#include <algorithm>
#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "Range.h"
#include "glm_utils.h"
#include "mesh/vertex_index_range.h"

namespace mesh  {

uint32_t find_min_vertex_index(const std::span<const glm::uvec3> triangles) {
    if (triangles.empty()) {
        return 0;
    }

    uint32_t min_vertex = UINT32_MAX;
    for (const glm::uvec3 &triangle : triangles) {
        min_vertex = std::min(glm::compMin(triangle), min_vertex);
        if (min_vertex == 0) {
            return 0;
        }
    }
    return min_vertex;
}

uint32_t find_max_vertex_index(const std::span<const glm::uvec3> triangles) {
    if (triangles.empty()) {
        return 0;
    }

    uint32_t max_vertex = 0;
    for (const glm::uvec3 &triangle : triangles) {
        max_vertex = std::max(glm::compMax(triangle), max_vertex);
    }
    return max_vertex;
}

Range<uint32_t> find_vertex_index_range(const std::span<const glm::uvec3> triangles) {
    if (triangles.empty()) {
        return {};
    }

    uint32_t min_vertex = UINT32_MAX;
    uint32_t max_vertex = 0;
    for (const glm::uvec3 &triangle : triangles) {
        min_vertex = glm::compMin(glm::uvec4(triangle, min_vertex));
        max_vertex = glm::compMax(glm::uvec4(triangle, max_vertex));
    }
    return {min_vertex, max_vertex + 1};
}

uint32_t compute_vertex_count(const std::span<const glm::uvec3> triangles) {
    const uint32_t max_vertex = find_max_vertex_index(triangles);
    if (max_vertex <= triangles.size() * 3) {
        // Dense vertex range
        std::vector<bool> visited(max_vertex + 1, false);
        for (const glm::uvec3 &triangle : triangles) {
            for (const uint32_t vertex : iterate(triangle)) {
                visited[vertex] = true;
            }
        }
        return std::count(visited.begin(), visited.end(), true);
    } else {
        // Sparse vertex range
        std::unordered_set<uint32_t> visited;
        for (const glm::uvec3 &triangle : triangles) {
            for (const uint32_t vertex : iterate(triangle)) {
                visited.insert(vertex);
            }
        }
        return visited.size();
    }
}

}
