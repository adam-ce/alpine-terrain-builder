#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>
#include <radix/geometry.h>

#include "PlaneFrame.h"
#include "SegmentedBuffer.h"
#include "StampSet.h"
#include "geometry/geometry.h"
#include "mesh/topology/adjacency.h"
#include "range_utils.h"

struct CorrespondenceOptions {
    // Lateral excursion allowed outside an output triangle, as a fraction of its longest edge.
    double slack_ratio = 0.25;
    // Vertices outside the output triangle a walk may pass through before a branch is abandoned.
    uint32_t max_steps_outside = 2;
    // Below this, a source triangle faces away and belongs to another layer of the surface.
    double min_normal_dot = 0.0;
};

// Source triangles that may contribute to each output triangle, indexed by output triangle.
using Correspondence = SegmentedBuffer<uint32_t, uint32_t, uint32_t>;

namespace detail {

inline double longest_edge_of(const radix::geometry::Triangle<3, double> &corners) {
    return std::sqrt(std::max({
        glm::distance2(corners[0], corners[1]),
        glm::distance2(corners[1], corners[2]),
        glm::distance2(corners[2], corners[0]),
    }));
}

// A vertex waiting to be walked, and the vertices outside the outline passed through to reach it.
struct PendingVertex {
    uint32_t steps_outside = 0;
    uint32_t vertex = 0;
    auto operator<=>(const PendingVertex &) const = default;
};

// The surface before simplification, with the two adjacencies the walk and the filter read.
struct VertexGraph {
    std::span<const glm::uvec3> triangles;
    std::span<const glm::dvec3> positions;
    std::vector<std::vector<uint32_t>> vertex_adjacency;
    std::vector<std::vector<uint32_t>> vertex_to_triangles;
};

inline VertexGraph build_vertex_graph(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions) {
    return VertexGraph{
        .triangles = triangles,
        .positions = positions,
        .vertex_adjacency = mesh::build_vertex_adjacency(triangles, positions.size()),
        .vertex_to_triangles = mesh::create_vertex_to_triangle_mapping(triangles, positions.size()),
    };
}

// An output triangle laid onto its own plane, the region source geometry is measured against.
struct OutputTriangle {
    glm::uvec3 vertices;
    PlaneFrame frame;
    radix::geometry::Triangle<2, double> outline;
    double longest_edge = 0.0;
};

// Empty for a degenerate output triangle, which spans no plane to measure against.
inline std::optional<OutputTriangle> flatten_output_triangle(const glm::uvec3 &triangle, const std::span<const glm::dvec3> positions) {
    const radix::geometry::Triangle<3, double> output_corners = geometry::corners(triangle, positions);
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(output_corners);
    if (!frame) {
        return std::nullopt;
    }
    return OutputTriangle{
        .vertices = triangle,
        .frame = *frame,
        .outline = frame->flatten(output_corners),
        .longest_edge = longest_edge_of(output_corners),
    };
}

using PendingQueue = std::priority_queue<PendingVertex, std::vector<PendingVertex>, std::greater<>>;

// Walks outward from the output triangle's corners, and finds the connected vertices near it.
inline void collect_relevant_vertices(
    const OutputTriangle &output,
    const VertexGraph &source,
    const CorrespondenceOptions &options,
    std::vector<uint32_t> &reached,
    StampSet &visited,
    PendingQueue &pending) {
    const double allowed_excursion = options.slack_ratio * output.longest_edge;

    visited.reset(source.positions.size());
    reached.clear();

    // The corners are seeded rather than measured, so rounding cannot cost them a step.
    for (const uint8_t corner : range<uint8_t>(3)) {
        pending.push(PendingVertex{.steps_outside = 0, .vertex = output.vertices[corner]});
    }

    while (!pending.empty()) {
        const PendingVertex current = pending.top();
        pending.pop();
        if (!visited.insert(current.vertex)) {
            // Reached again by a costlier route, after the cheapest one already took it.
            continue;
        }
        reached.push_back(current.vertex);

        for (const uint32_t neighbour : source.vertex_adjacency[current.vertex]) {
            if (visited.contains(neighbour)) {
                continue;
            }

            const double offset = geometry::distance_to_triangle(output.frame.project(source.positions[neighbour]), output.outline);
            if (offset > allowed_excursion) {
                continue;
            }
            const uint32_t steps_outside = current.steps_outside + (offset > 0.0 ? 1 : 0);
            if (steps_outside > options.max_steps_outside) {
                continue;
            }

            pending.push(PendingVertex{.steps_outside = steps_outside, .vertex = neighbour});
        }
    }
}

// Appends the triangles touching the found vertices that face the output triangle and cover part of it.
inline void collect_covering_triangles(
    const OutputTriangle &output,
    const VertexGraph &source,
    const CorrespondenceOptions &options,
    const std::span<const uint32_t> reached,
    StampSet &visited,
    Correspondence &correspondence) {
    visited.reset(source.triangles.size());

    for (const uint32_t vertex : reached) {
        for (const uint32_t triangle_index : source.vertex_to_triangles[vertex]) {
            if (!visited.insert(triangle_index)) {
                continue;
            }

            const radix::geometry::Triangle<3, double> source_corners = geometry::corners(source.triangles[triangle_index], source.positions);
            if (glm::dot(radix::geometry::normal(source_corners), output.frame.normal) <= options.min_normal_dot) {
                // Another layer of the surface, and another output triangle's concern.
                continue;
            }
            if (geometry::triangles_overlap(output.outline, output.frame.flatten(source_corners))) {
                correspondence.push_to_last_segment(triangle_index);
            }
        }
    }
}

} // namespace detail

// Gathers, for every output triangle, the source triangles that may have contributed to it.
[[nodiscard]]
inline Correspondence find_source_triangles(
    const std::span<const glm::uvec3> source_triangles,
    const std::span<const glm::uvec3> output_triangles,
    const std::span<const glm::dvec3> positions,
    const CorrespondenceOptions options = {}) {
    const detail::VertexGraph source = detail::build_vertex_graph(source_triangles, positions);

    StampSet visited_vertices(positions.size());
    StampSet visited_triangles(source_triangles.size());

    detail::PendingQueue pending;
    std::vector<uint32_t> reached; // vertices, of the output triangle being walked

    Correspondence correspondence;
    for (const glm::uvec3 &triangle : output_triangles) {
        correspondence.start_new_segment();

        const std::optional<detail::OutputTriangle> output = detail::flatten_output_triangle(triangle, positions);
        if (!output) {
            continue;
        }

        detail::collect_relevant_vertices(*output, source, options, reached, visited_vertices, pending);
        detail::collect_covering_triangles(*output, source, options, reached, visited_triangles, correspondence);
    }

    return correspondence;
}
