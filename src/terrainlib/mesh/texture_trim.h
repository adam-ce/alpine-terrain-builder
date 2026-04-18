#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

struct TrimResult {
    cv::Mat texture;
    std::vector<glm::dvec2> uvs;
};

TrimResult trim_texture(const cv::Mat &texture, const std::span<const glm::dvec2> uvs, const uint32_t padding = 2);
void trim_texture_inplace(cv::Mat &texture, std::span<glm::dvec2> uvs, const uint32_t padding = 2);

template <glm::length_t n_dims>
void trim_texture_inplace(mesh::Simple_<n_dims, double> &mesh) {
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        return;
    }
    trim_texture_inplace(mesh.texture.value(), mesh.uvs);
} 
template <glm::length_t n_dims, typename T>
void trim_texture_inplace(mesh::View_<n_dims, double> &mesh) {
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        return;
    }
    trim_texture_inplace(mesh.texture.value(), mesh.uvs);
}
