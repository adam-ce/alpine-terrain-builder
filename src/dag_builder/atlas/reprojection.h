#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "PlaneFrame.h"
#include "atlas/pull_reproject_texture.h"
#include "correspondence.h"
#include "geometry_utils.h"
#include "polygon/clip.h"
#include "range_utils.h"
#include "uv/atlas.h"

// A source triangle's uv corners and the map they address, mirroring MappedTriangle.
struct SourceMapping {
    uint32_t map_index = 0;
    glm::uvec3 uvs; // indices into uv_maps[map_index]
};

// The surface before simplification, in the uv spaces the bake reads back through.
struct SourceSurface {
    std::span<const glm::uvec3> triangles;
    std::span<const glm::dvec3> positions;
    std::span<const SourceMapping> mapping; // per triangle
    std::span<const std::vector<glm::dvec2>> uv_maps;
};

namespace detail {

// An output triangle on its own plane, together with the atlas patch it was given.
struct FlatTarget {
    PlaneFrame frame;
    radix::geometry::Triangle<2, double> projected;
    std::array<glm::dvec2, 3> uvs;
};

// A source triangle on the target's plane, with the height it sits at.
struct FlatSource {
    radix::geometry::Triangle<2, double> projected;
    glm::dvec3 heights; // per corner, off the target plane
    std::array<glm::dvec2, 3> uvs;
    uint32_t map_index = 0;
};

// A part of an output triangle lying under exactly one source triangle.
struct Fragment {
    double height = 0.0;
    ReprojectionTriangle triangle;
};

// Empty for a degenerate output triangle, which spans no plane to measure against.
inline std::optional<FlatTarget> flatten_target(
    const uv::Atlas &atlas,
    const uint32_t triangle_index,
    const std::span<const glm::dvec3> positions) {
    const glm::uvec3 triangle = atlas.triangles[triangle_index];

    glm::uvec3 vertices{};
    std::array<glm::dvec2, 3> uvs{};
    for (const uint8_t corner : range<uint8_t>(3)) {
        vertices[corner] = atlas.vertex_map[triangle[corner]];
        uvs[corner] = atlas.uvs[triangle[corner]];
    }

    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(vertices, positions);
    if (!frame) {
        return std::nullopt;
    }
    return FlatTarget{
        .frame = *frame,
        .projected = frame->flatten(vertices, positions),
        .uvs = uvs,
    };
}

inline FlatSource flatten_source(
    const SourceSurface &source,
    const uint32_t triangle_index,
    const PlaneFrame &frame) {
    const glm::uvec3 triangle = source.triangles[triangle_index];
    const SourceMapping &mapping = source.mapping[triangle_index];
    const std::vector<glm::dvec2> &map_uvs = source.uv_maps[mapping.map_index];

    FlatSource flat;
    flat.map_index = mapping.map_index;
    for (const uint8_t corner : range<uint8_t>(3)) {
        const glm::dvec3 position = source.positions[triangle[corner]];
        flat.projected[corner] = frame.project(position);
        flat.heights[corner] = frame.distance_to(position);
        flat.uvs[corner] = map_uvs[mapping.uvs[corner]];
    }
    return flat;
}

// Cuts the output triangle against one source triangle, and describes each part in both uv spaces.
inline void clip_against_source(
    const FlatTarget &target,
    const FlatSource &source,
    std::vector<polygon::Triangle2d> &overlap,
    std::vector<Fragment> &fragments) {
    overlap.clear();
    polygon::clip_triangle(target.projected, source.projected, overlap);

    for (const polygon::Triangle2d &part : overlap) {
        Fragment fragment;
        fragment.triangle.source_image_index = source.map_index;
        for (const uint8_t corner : range<uint8_t>(3)) {
            const glm::dvec3 source_weights = compute_barycentric(part[corner], source.projected);
            const glm::dvec3 target_weights = compute_barycentric(part[corner], target.projected);
            fragment.triangle.source_uvs[corner] = interpolate(source.uvs, source_weights);
            fragment.triangle.target_uvs[corner] = interpolate(target.uvs, target_weights);
        }

        // TODO: Sort is centroid based and will be wrong for intersecting parts
        const glm::dvec2 centre = (part[0] + part[1] + part[2]) / 3.0;
        fragment.height = std::abs(glm::dot(compute_barycentric(centre, source.projected), source.heights));

        fragments.push_back(fragment);
    }
}

} // namespace detail

// Prepare the triangles for the bake based on the correspondances and atlas.
[[nodiscard]]
inline std::vector<ReprojectionTriangle> build_reprojection_triangles(
    const uv::Atlas &atlas,
    const Correspondence &correspondence,
    const SourceSurface &source,
    const std::span<const glm::dvec3> output_positions) {
    DEBUG_ASSERT(correspondence.segment_count() == atlas.triangles.size());

    std::vector<ReprojectionTriangle> reprojection;
    std::vector<detail::Fragment> fragments;
    std::vector<polygon::Triangle2d> overlap;

    for (const uint32_t triangle_index : range<uint32_t>(atlas.triangles.size())) {
        if (std::ranges::binary_search(atlas.unmapped_triangles, triangle_index)) {
            continue;
        }
        const std::optional<detail::FlatTarget> target =
            detail::flatten_target(atlas, triangle_index, output_positions);
        if (!target) {
            continue;
        }

        fragments.clear();
        for (const uint32_t source_index : correspondence.segment(triangle_index)) {
            const detail::FlatSource flat = detail::flatten_source(source, source_index, target->frame);
            detail::clip_against_source(*target, flat, overlap, fragments);
        }

        // Farthest first, so the nearest surface is the one left standing where they overlap.
        std::ranges::stable_sort(fragments, std::greater<>{}, &detail::Fragment::height);
        for (const detail::Fragment &fragment : fragments) {
            reprojection.push_back(fragment.triangle);
        }
    }

    return reprojection;
}