#include <glm/gtx/norm.hpp>
#include <glm/common.hpp>

#include "polygon/utils.h"

namespace polygon {
bool is_planar(const Polygon3d &polygon, const double epsilon) {
    if (polygon.size() < 4) {
        return true; // 3 points always define a plane
    }

    glm::dvec3 p0 = polygon.points[0];
    glm::dvec3 normal(0);

    // find non-collinear triple (to compute normal)
    size_t i = 1;
    while (i + 1 < polygon.size()) {
        const glm::dvec3 v1 = polygon.points[i] - p0;
        const glm::dvec3 v2 = polygon.points[i + 1] - p0;
        normal = glm::cross(v1, v2);
        if (glm::length2(normal) > epsilon * epsilon) {
            break;
        }
        i++;
    }
    if (glm::length2(normal) <= epsilon * epsilon) {
        // all points collinear
        return true;
    }

    normal = glm::normalize(normal);

    // check all points
    for (const auto &p : polygon.points) {
        const double distance = std::abs(glm::dot(p - p0, normal));
        if (distance > epsilon) {
            return false;
        }
    }
    
    return true;
}
}
