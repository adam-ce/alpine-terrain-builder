#pragma once

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

namespace {
glm::dvec2 cartesian_to_lon_lat(const glm::dvec3 &point) {
    const glm::dvec3 normalized = glm::normalize(point);
    const double lon = std::atan2(normalized.y, normalized.x);
    const double lat = std::asin(normalized.z);
    return glm::dvec2(lon, lat);
}

glm::dvec3 lon_lat_to_cartesian(const glm::dvec2 &lon_lat) {
    const double lon = lon_lat.x;
    const double lat = lon_lat.y;

    const double cos_lat = std::cos(lat);
    return glm::dvec3(
        cos_lat * std::cos(lon),
        cos_lat * std::sin(lon),
        std::sin(lat));
}
}

class SphereProjector {
public:
    explicit SphereProjector(glm::dvec3 tangent_point) {
        // Set up local tangent frame
        this->radius = glm::length(tangent_point);
        const glm::dvec3 a = tangent_point / radius;
        const glm::dvec3 not_parallel_to_normal = a.z < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(0, 1, 0);
        const glm::dvec3 b = glm::normalize(glm::cross(a, not_parallel_to_normal));
        const glm::dvec3 c = glm::normalize(glm::cross(a, b));

        // Build rotation matrix (local tangent space basis)
        this->rotation = glm::mat3(a, b, c);
        this->inv_rotation = glm::transpose(this->rotation);
    }

    glm::dvec2 project_point(const glm::dvec3 &cartesian) const {
        // TODO: project onto tangent plane instead?
        const glm::dvec3 rotated = glm::normalize(rotation * cartesian);
        return cartesian_to_lon_lat(rotated);
    }

    glm::dvec3 unproject_point(const glm::dvec2 &lon_lat) const {
        const glm::dvec3 cartesian = lon_lat_to_cartesian(lon_lat);
        const glm::dvec3 unrotated_point = inv_rotation * cartesian;
        const glm::dvec3 point_on_sphere = unrotated_point * radius;
        return point_on_sphere;
    }

private:
    double radius;
    glm::mat3 rotation;
    glm::mat3 inv_rotation;
};
