#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "mesh/geometry.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "range_utils.h"

namespace uv {

struct AtlasOptions {
    // Texels the packer keeps between charts, for a bake to dilate into.
    uint32_t padding = 2;
};

// Texel size the chart scale is fitted to. The packed atlas lands near this, not on it.
inline constexpr uint32_t DEFAULT_PACKING_RESOLUTION = 1024;

// A uv layout minted on the mesh, with the vertex duplication its chart seams force.
struct Atlas {
    std::vector<glm::uvec3> triangles; // one per input triangle, in input order
    std::vector<glm::dvec2> uvs; // per duplicated vertex, in [0, 1]
    std::vector<uint32_t> vertex_map; // duplicated vertex -> input vertex
    std::vector<uint32_t> unmapped_triangles; // degenerate or nan, left with no uv area
    glm::uvec2 size{0}; // packed texel size, what the uvs are relative to
    uint32_t chart_count = 0;

    // Fraction of the packed atlas the charts cover. The uvs are normalised, so it is just their area.
    [[nodiscard]] double utilization() const {
        return sum(this->triangles, [&](const glm::uvec3 &triangle) {
            return compute_triangle_area(triangle, this->uvs);
        });
    }
    [[nodiscard]] double aspect() const {
        return double(this->size.x) / this->size.y;
    }
};

// Cuts the mesh into charts and packs them into one atlas. Takes any triangle soup, manifold or not.
Atlas build_atlas(
    std::span<const glm::uvec3> triangles,
    std::span<const glm::dvec3> positions,
    uint32_t packing_resolution = DEFAULT_PACKING_RESOLUTION,
    AtlasOptions options = {});

inline Atlas build_atlas(const mesh::View &mesh, const uint32_t packing_resolution = DEFAULT_PACKING_RESOLUTION, const AtlasOptions options = {}) {
    return build_atlas(mesh.triangles, mesh.positions, packing_resolution, options);
}

inline Atlas build_atlas(const mesh::Simple &mesh, const uint32_t packing_resolution = DEFAULT_PACKING_RESOLUTION, const AtlasOptions options = {}) {
    return build_atlas(mesh.triangles, mesh.positions, packing_resolution, options);
}

} // namespace uv
