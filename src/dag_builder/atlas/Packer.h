#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include <glm/glm.hpp>

#include "number_utils.h"
#include "SegmentedBuffer.h"

namespace atlas {

struct UvMesh {
    std::span<const glm::uvec3> triangles;
    std::span<const glm::dvec2> uvs;
    glm::uvec2 texture_size;
};

class Packing {
public:
    Packing() = default;
    Packing(std::vector<glm::dvec2> uvs, const double effective_pixel_area, const float utilization = 1.0f)
        : Packing(SegmentedBuffer<glm::dvec2>(std::move(uvs)), effective_pixel_area, utilization) {}
    Packing(SegmentedBuffer<glm::dvec2> uvs, const double effective_pixel_area, const float utilization = 1.0f)
        : _uvs(std::move(uvs)), _effective_pixel_area(effective_pixel_area), _utilization(utilization) {}

    std::span<const glm::dvec2> uvs_for_mesh(const uint32_t mesh_index) const {
        return this->_uvs.segment(mesh_index);
    }

    std::vector<glm::dvec2>& uvs() {
        return this->_uvs.backing();
    }
    const std::vector<glm::dvec2> &uvs() const {
        return this->_uvs.backing();
    }

    // Sum of effective input pixels (source-texture pixels actually covered by UVs) across all packed meshes.
    double effective_pixel_area() const {
        return this->_effective_pixel_area;
    }

    // Fraction of the packed atlas actually covered by charts. 1.0 for
    // packings that were not produced by the chart packer (e.g. a single
    // mesh's own UVs), since there is no wasted atlas space to account for.
    float utilization() const {
        return this->_utilization;
    }

private:
    SegmentedBuffer<glm::dvec2> _uvs;
    double _effective_pixel_area = 0.0;
    float _utilization = 1.0f;
};

class Packer {
public:
    Packer();
    ~Packer();

    void add_uv_mesh(const UvMesh &mesh) const;
    void add_uv_mesh(const std::span<const glm::uvec3> triangles,
                     const std::span<const glm::dvec2> uvs,
                     const glm::uvec2 texture_size) const {
        return this->add_uv_mesh({triangles, uvs, texture_size});
    }
    Packing pack() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

};

} // namespace atlas
