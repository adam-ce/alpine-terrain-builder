#pragma once

#include <span>
#include <vector>
#include <ranges>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/manifold.h"
#include "uv.h"
#include "reproject_texture.h"
#include "atlas/atlas.h"
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
        return compute_surface_area(mesh::View(mesh));
    });

    const double total_area = sum(areas);

    std::vector<PackingMetric> result;
    result.reserve(meshes.size());
    for (size_t i = 0; i < meshes.size(); i++) {
        const auto& mesh = meshes[i];
        const double area = areas[i];
        const double relative_area = (total_area > 0.0) ? (area / total_area) : 0.0;

        const radix::geometry::Aabb2d uv_bounds = calculate_bounds(mesh.uvs);
        const glm::dvec2 uv_size = uv_bounds.size();
        const double aspect_ratio = std::max(uv_size.x / uv_size.y, 1e-6);

        result.push_back(PackingMetric{aspect_ratio, relative_area});
    }
    return result;
}

atlas::Plan plan_from_metrics(const std::span<const PackingMetric> metrics, const glm::uvec2 &texture_size) {
    const size_t total_pixels = static_cast<size_t>(texture_size.x) * texture_size.y;
    const double pixel_budget = static_cast<double>(total_pixels);

    const std::vector<glm::uvec2> target_sizes = transform_vector(metrics, [&](const PackingMetric&metric) {
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

/*
struct [[nodiscard]] UnwrapAtlasResult {
    Texture texture;
    std::vector<glm::dvec2> uvs;
};

inline void unwrap_atlas(const mesh::View mesh) {
    ASSERT(mesh.has_uvs());
    ASSERT(mesh.has_texture());

    const uint32_t vertex_count = mesh.positions.size();
    const uint32_t triangle_count = mesh.triangles.size();
    const Texture base_texture = mesh.texture.value();
    const glm::uvec2 base_texture_size(base_texture.cols, base_texture.rows);

    const auto component_index = mesh::find_connected_components(mesh.triangles, vertex_count);
    const auto &[vertex_to_component, component_count] = component_index;
    if (component_count == 1) {
        Texture texture = cv::Mat::zeros(base_texture_size.y, base_texture_size.x, base_texture.type());
        std::vector<glm::dvec2> new_uvs = uv::unwrap(mesh).value();
        reproject_texture(mesh.triangles, mesh.uvs, new_uvs, mesh.texture, texture);
        return; // UnwrapAtlasResult{ texture, new_uvs };
    }

    const std::vector<mesh::Simple> components = mesh::split_into_connected_components(mesh, component_index);

    std::vector<glm::uvec2> texture_sizes;
    texture_sizes.reserve(component_count);
    const uint32_t base_texture_size_max_side = glm::compMax(base_texture_size);
    for (const mesh::Simple &component : components) {
        texture_sizes.emplace_back((base_texture_size_max_side * component.vertex_count()) / vertex_count);
    }
    const atlas::Plan plan = atlas::plan(texture_sizes);
    Texture texture = cv::Mat::zeros(plan.size.y, plan.size.x, base_texture.type());

    for (uint32_t component_index = 0; component_index < component_count; component_index++) {
        const mesh::Simple &component = components[component_index];
        const std::vector<glm::dvec2> old_uvs = std::move(component.uvs);
        DEBUG_ASSERT(is_manifold(component));
        component.uvs = uv::unwrap(component).value();
        atlas::map_uvs(plan, component_index, component.uvs);
        reproject_texture(component.triangles, old_uvs, component.uvs, base_texture, texture);
    }
    return UnwrapAtlasResult{texture, new_uvs};
}
*/