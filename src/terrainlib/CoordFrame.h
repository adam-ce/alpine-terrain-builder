#pragma once

#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <radix/geometry.h>

// An orthonormal frame on a surface.
template <typename T = double>
struct CoordFrame_ {
    glm::vec<3, T> origin;
    glm::vec<3, T> tangent;
    glm::vec<3, T> bitangent;
    glm::vec<3, T> normal;

    // The position in this frame's coordinates.
    glm::vec<3, T> to_local(const glm::vec<3, T> &position) const {
        const glm::vec<3, T> offset = position - this->origin;
        return glm::vec<3, T>(
            glm::dot(offset, this->tangent),
            glm::dot(offset, this->bitangent),
            glm::dot(offset, this->normal));
    }

    // The inverse: a position given in this frame's coordinates, back in world coordinates.
    glm::vec<3, T> to_world(const glm::vec<3, T> &local) const {
        return this->origin
            + local.x * this->tangent
            + local.y * this->bitangent
            + local.z * this->normal;
    }
};

using CoordFrame = CoordFrame_<double>;