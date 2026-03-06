#pragma once

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <span>
#include <vector>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "mesh/manifold.h"
#include "uv.h"
#include "reproject_texture.h"
#include "atlas/atlas.h"

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
