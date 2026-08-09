#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "geometry/PlaneFrame.h"
#include "atlas/BakeSource.h"
#include "atlas/pull_reproject_texture.h"
#include "correspondence.h"
#include "enumerate.h"
#include "geometry/geometry.h"
#include "polygon/clip.h"
#include "range_utils.h"

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
    const glm::uvec3 &triangle,
    const std::span<const glm::dvec2> uvs,
    const std::span<const glm::dvec3> positions) {
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle, positions);
    if (!frame) {
        return std::nullopt;
    }
    return FlatTarget{
        .frame = *frame,
        .projected = frame->flatten(triangle, positions),
        .uvs = {uvs[triangle.x], uvs[triangle.y], uvs[triangle.z]},
    };
}

inline FlatSource flatten_source(
    const BakeSource &source,
    const uint32_t triangle_index,
    const PlaneFrame &frame) {
    const glm::uvec3 triangle = source.triangles[triangle_index];
    const UvRef &mapping = source.uv_triangles[triangle_index];
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
            const glm::dvec3 source_weights = geometry::compute_barycentric(part[corner], source.projected);
            const glm::dvec3 target_weights = geometry::compute_barycentric(part[corner], target.projected);
            fragment.triangle.source_uvs[corner] = geometry::interpolate(source.uvs, source_weights);
            fragment.triangle.target_uvs[corner] = geometry::interpolate(target.uvs, target_weights);
        }

        // TODO: Sort is centroid based and will be wrong for intersecting parts
        const glm::dvec2 centre = (part[0] + part[1] + part[2]) / 3.0;
        fragment.height = std::abs(glm::dot(geometry::compute_barycentric(centre, source.projected), source.heights));

        fragments.push_back(fragment);
    }
}

} // namespace detail

// Prepare the triangles for the bake based on the correspondances and the target uv layout.
[[nodiscard]]
inline std::vector<ReprojectionTriangle> build_reprojection_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs,
    const std::span<const glm::dvec3> positions,
    const Correspondence &correspondence,
    const BakeSource &source) {
    DEBUG_ASSERT(correspondence.segment_count() == triangles.size());

    std::vector<ReprojectionTriangle> reprojection;
    // At least one fragment per output triangle, more where sources overlap it.
    reprojection.reserve(triangles.size());
    std::vector<detail::Fragment> fragments;
    std::vector<polygon::Triangle2d> overlap;

    for (const auto [triangle_index, triangle] : enumerate(triangles)) {
        const std::optional<detail::FlatTarget> target = detail::flatten_target(triangle, uvs, positions);
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