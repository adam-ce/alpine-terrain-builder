#include <algorithm>

#include <glm/glm.hpp>

#include "mesh/normalize.h"

namespace mesh {

inline constexpr bool compare_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    // First, compare by x
    if (t1.x != t2.x) {
        return t1.x < t2.x;
    }

    // If x is equal, compare by y
    if (t1.y != t2.y) {
        return t1.y < t2.y;
    }

    // If x and y are equal, compare by z
    return t1.z < t2.z;
}

inline bool compare_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return compare_triangles(normalize_triangle(t1), normalize_triangle(t2));
}

inline bool compare_equality_triangles(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return normalize_triangle(t1) == normalize_triangle(t2);
}
inline bool compare_equality_triangles_ignore_orientation(const glm::uvec3 &t1, const glm::uvec3 &t2) {
    return std::is_permutation(&t1.x, &t1.z + 1, &t2.x);
}

}
