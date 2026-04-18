#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace mesh {

struct BFS {
    std::vector<uint32_t> parent;
    std::vector<uint32_t> dist;
    std::vector<std::vector<uint32_t>> adjacency;
    static constexpr uint32_t invalid = -1;
};

BFS run_bfs(const uint32_t source, const std::span<const glm::uvec3> triangles);
BFS run_bfs(const uint32_t source, std::vector<std::vector<uint32_t>> adjacency);

std::vector<uint32_t> reconstruct_path(const uint32_t start, const uint32_t end, const std::vector<uint32_t> &parent);

}
