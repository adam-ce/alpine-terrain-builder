#pragma once

#include <span>
#include <functional>

#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"
#include "atlas/LayoutPlanner.h"
#include "atlas/Layout.h"
#include "atlas/Sheet.h"

namespace atlas {

inline Sheet build(
    const std::span<const std::reference_wrapper<const SimpleMesh>> meshes,
    const Layout &layout) {
    Sheet result;
    result.uvs.resize(meshes.size());

    if (layout.slots.empty()) {
        // Create single pixel empty texture
        result.texture = cv::Mat(1, 1, CV_8UC4, cv::Scalar(0, 0, 0, 0));
        for (size_t i = 0; i < meshes.size(); i++) {
            const SimpleMesh &mesh = meshes[i];
            result.uvs[i].assign(mesh.vertex_count(), glm::dvec2(0.0, 0.0));
        }
        return result;
    }

    const int atlas_type = meshes[layout.slots[0].mesh_index].get().texture->type();
    cv::Mat atlas_image(layout.atlas_size.y, layout.atlas_size.x, atlas_type, cv::Scalar(0, 0, 0, 0));

    for (const auto &slot : layout.slots) {
        const SimpleMesh &mesh = meshes[slot.mesh_index];
        result.uvs[slot.mesh_index].resize(mesh.vertex_count());

        // copy texture into atlas
        cv::Mat roi(atlas_image,
                    cv::Rect(
                        static_cast<int>(slot.pixel_position.x),
                        static_cast<int>(slot.pixel_position.y),
                        mesh.texture->cols,
                        mesh.texture->rows));
        mesh.texture->copyTo(roi);

        if (!mesh.uvs.empty()) {
            for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
                const glm::dvec2 &uv = mesh.uvs[vertex_index];
                result.uvs[slot.mesh_index][vertex_index] = glm::dvec2(
                    slot.uv_space.position.x + uv.x * slot.uv_space.size.x,
                    slot.uv_space.position.y + uv.y * slot.uv_space.size.y);
                DEBUG_ASSERT(result.uvs[slot.mesh_index][vertex_index].x >= 0.0 && result.uvs[slot.mesh_index][vertex_index].x <= 1.0);
                DEBUG_ASSERT(result.uvs[slot.mesh_index][vertex_index].y >= 0.0 && result.uvs[slot.mesh_index][vertex_index].y <= 1.0);
            }
        } else {
            for (size_t vertex_index = 0; vertex_index < mesh.vertex_count(); vertex_index++) {
                result.uvs[slot.mesh_index][vertex_index] = glm::dvec2(0.0, 0.0);
            }
        }
    }

    result.texture = atlas_image;
    return result;
}

}
