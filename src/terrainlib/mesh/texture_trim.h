#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/View.h"
#include "range_utils.h"

struct TrimResult {
    cv::Mat texture;
    std::vector<glm::dvec2> uvs;
};

constexpr uint32_t DefaultPadding = 2;

// one-pass trimming
TrimResult trim_texture(
    const cv::Mat &texture,
    const std::span<const glm::dvec2> uvs,
    const uint32_t padding = DefaultPadding);

void trim_texture_inplace(
    cv::Mat &texture,
    std::span<glm::dvec2> uvs,
    uint32_t padding = DefaultPadding);

template <glm::length_t n_dims>
void trim_texture_inplace(mesh::Simple_<n_dims, double> &mesh) {
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        return;
    }
    trim_texture_inplace(mesh.texture.value(), mesh.uvs);
}

template <glm::length_t n_dims>
void trim_texture_inplace(mesh::View_<n_dims, double> &mesh) {
    if (!mesh.has_texture() || !mesh.has_uvs()) {
        return;
    }
    trim_texture_inplace(mesh.texture.value(), mesh.uvs);
}

// two-pass trimming
namespace detail {
inline void check_uv(const glm::dvec2 &uv) {
    DEBUG_ASSERT(uv.x >= 0.0 && uv.x <= 1.0);
    DEBUG_ASSERT(uv.y >= 0.0 && uv.y <= 1.0);
}
}

struct UvRemap {
    glm::dvec2 offset = glm::dvec2(0.0);
    glm::dvec2 scale = glm::dvec2(1.0);

    static UvRemap identity() {
        return UvRemap{};
    }

    bool is_identity() const {
        return this->scale == glm::dvec2(1.0) && this->offset == glm::dvec2(0.0);
    }
    glm::dvec2 remap_uv(glm::dvec2 uv) const {
        this->remap_uv_inplace(uv);
        return uv;
    }
    void remap_uv_inplace(glm::dvec2 &uv) const {
        detail::check_uv(uv);
        uv = (uv - this->offset) / this->scale;
        uv = glm::clamp(uv, glm::dvec2(0.0), glm::dvec2(1.0));
        detail::check_uv(uv);
    }
    void remap_uvs(
        const std::span<const glm::dvec2> input_uvs,
        std::span<glm::dvec2> output_uvs) const {
        DEBUG_ASSERT(input_uvs.size() == output_uvs.size());
        if (this->is_identity()) {
            std::copy(input_uvs.begin(), input_uvs.end(), output_uvs.begin());
            return;
        }

        for (const size_t i : range(input_uvs.size())) {
            output_uvs[i] = this->remap_uv(input_uvs[i]);
        }
    }
    std::vector<glm::dvec2> remap_uvs(const std::span<const glm::dvec2> uvs) const {
        if (this->is_identity()) {
            return std::vector<glm::dvec2>(uvs.begin(), uvs.end());
        }

        std::vector<glm::dvec2> remapped;
        remapped.reserve(uvs.size());
        for (const glm::dvec2 uv : uvs) {
            remapped.push_back(this->remap_uv(uv));
        }
        return remapped;
    }
    void remap_uvs_inplace(std::span<glm::dvec2> uvs) const {
        if (this->is_identity()) {
            return;
        }

        for (glm::dvec2 &uv : uvs) {
            this->remap_uv_inplace(uv);
        }
    }
};

struct TextureTrim {
    cv::Mat texture;
    UvRemap uv_remap;
};

TextureTrim compute_texture_trim(const cv::Mat &input_texture, const std::span<const glm::dvec2> input_uvs, const uint32_t padding = DefaultPadding);
TextureTrim compute_texture_trim(const cv::Mat &input_texture, const radix::geometry::Aabb2d &uv_bounds, const uint32_t padding = DefaultPadding);
