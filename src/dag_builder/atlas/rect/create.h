#pragma once

#include <span>
#include <vector>
#include <ranges>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/topology/manifold.h"
#include "atlas/rect/atlas.h"
#include "mesh/geometry.h"
#include "mesh/bounds.h"
#include "range_utils.h"

struct PackingMetric {
    double aspect_ratio;
    double relative_area;
};

template <std::ranges::input_range Range>
std::vector<PackingMetric> calculate_packing_metrics(Range &&meshes) {
    const std::vector<double> areas = transform_vector(meshes, [&](const auto &mesh) {
        return mesh::compute_surface_area(mesh::View(mesh));
    });

    const double total_area = sum(areas);

    std::vector<PackingMetric> result;
    result.reserve(meshes.size());
    for (size_t i = 0; i < meshes.size(); i++) {
        const auto& mesh = meshes[i];
        const double area = areas[i];
        const double relative_area = (total_area > 0.0) ? (area / total_area) : 0.0;

        double aspect_ratio = 1;
        if (mesh.has_uvs()) {
            const radix::geometry::Aabb2d uv_bounds = mesh::calculate_bounds(mesh.uvs);
            const glm::dvec2 uv_size = uv_bounds.size();
            aspect_ratio = std::max(uv_size.x / uv_size.y, 1e-6);
        }

        result.push_back(PackingMetric{aspect_ratio, relative_area});
    }
    return result;
}

atlas::Plan plan_from_metrics(const std::span<const PackingMetric> metrics, const glm::uvec2 &texture_size) {
    const double pixel_budget = texture_size.x * texture_size.y;

    const std::vector<glm::uvec2> target_sizes = transform_vector(metrics, [&](const PackingMetric &metric) {
        const double target_pixel_area = metric.relative_area * pixel_budget;
        const double height = std::sqrt(target_pixel_area / metric.aspect_ratio);
        const double width = height * metric.aspect_ratio;

        return glm::max(glm::uvec2(1), glm::uvec2(glm::round(glm::dvec2(width, height))));
    });

    atlas::Plan plan = atlas::plan(target_sizes);
    atlas::rescale(plan, texture_size);
    return plan;
}

template <std::ranges::input_range Range>
atlas::Plan create_atlas_plan(const glm::uvec2 &texture_size, Range &&meshes) {
    const std::vector<PackingMetric> metrics = calculate_packing_metrics(meshes);
    return plan_from_metrics(metrics, texture_size);
}
