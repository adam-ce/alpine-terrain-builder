#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/cleanup.h"
#include "mesh/normalize.h"
#include "mesh/connectivity/adjacency.h"

namespace mesh {

namespace {
struct TriangleHash {
    size_t operator()(const glm::uvec3 &t) const {
        // we dont use hash::combine here since we want the hash to be order independent
        return std::hash<uint32_t>()(t.x) ^ std::hash<uint32_t>()(t.y) ^ std::hash<uint32_t>()(t.z);
    }
};

struct TriangleEquals {
    bool operator()(const glm::uvec3 &a, const glm::uvec3 &b) const noexcept {
        return normalize_triangle(a) == normalize_triangle(b);
    }
};
} // namespace

std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles) {
    std::vector<uint32_t> triangles_to_remove;
    std::unordered_set<glm::uvec3, TriangleHash, TriangleEquals> unique_triangles;

    for (uint32_t i = 0; i < triangles.size(); i++) {
        const glm::uvec3 &triangle = triangles[i];
        if (unique_triangles.find(triangle) != unique_triangles.end()) {
            triangles_to_remove.push_back(i);
        } else {
            unique_triangles.insert(triangle);
        }
    }

    return triangles_to_remove;
}

void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles) {
    remove_duplicate_triangles<double>(triangles, {}, false);
}

void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles) {
    triangles.erase(
        std::remove_if(triangles.begin(), triangles.end(), is_degenerate),
        triangles.end());
}

}
