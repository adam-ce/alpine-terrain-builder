#pragma once

#include <cstdint>
#include <span>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"

void trim_texture_inplace(cv::Mat &texture, std::span<glm::dvec2> uvs, const uint32_t padding = 2);

template <glm::length_t n_dims, typename T>
void trim_texture_inplace(SimpleMesh_<n_dims, T> &mesh) {
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        return;
    }
    trim_texture_inplace(*mesh.texture, mesh.uvs);
}
