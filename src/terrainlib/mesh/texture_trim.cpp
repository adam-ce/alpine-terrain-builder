#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/texture_trim.h"

namespace {
    bool is_empty(const radix::geometry::Aabb2i &bounds) {
        return bounds.min.x >= bounds.max.x || bounds.min.y >= bounds.max.y;
    }
}

void trim_texture_inplace(cv::Mat &texture, std::span<glm::dvec2> uvs, const uint32_t padding) {
    // Calculate the bounding box of the UVs in pixel space
    const radix::geometry::Aabb2d uv_bounds = radix::geometry::find_bounds(std::span<const glm::dvec2>(uvs));
    const glm::uvec2 texture_size(texture.cols, texture.rows);
    const radix::geometry::Aabb2i pixel_bounds = {
        glm::floor(uv_bounds.min * glm::dvec2(texture_size)),
        glm::ceil(uv_bounds.max * glm::dvec2(texture_size))};
    const radix::geometry::Aabb2i padded_pixel_bounds = {
        pixel_bounds.min - glm::ivec2(padding),
        pixel_bounds.max + glm::ivec2(padding)};
    const radix::geometry::Aabb2ui clamped_pixel_bounds = {
        glm::uvec2(glm::max(padded_pixel_bounds.min, glm::ivec2(0))),
        glm::uvec2(glm::min(padded_pixel_bounds.max, glm::ivec2(texture_size)))};
    if (is_empty(clamped_pixel_bounds)) {
        return;
    }

    // Crop the texture to the bounding box
    const cv::Rect roi(
        clamped_pixel_bounds.min.x,
        clamped_pixel_bounds.min.y,
        clamped_pixel_bounds.width(),
        clamped_pixel_bounds.height());
    texture = texture(roi).clone();
    // Update the UVs to match the cropped texture
    const glm::uvec2 cropped_texture_size(texture.cols, texture.rows);
    LOG_TRACE("Trimmed texture from {}x{} to {}x{}", texture_size.x, texture_size.y, cropped_texture_size.x, cropped_texture_size.y);
    const radix::geometry::Aabb2d cropped_uv_bounds = {
        glm::dvec2(clamped_pixel_bounds.min) / glm::dvec2(texture_size),
        glm::dvec2(clamped_pixel_bounds.max) / glm::dvec2(texture_size)};
    for (glm::dvec2 &uv : uvs) {
        uv = (uv - cropped_uv_bounds.min) / cropped_uv_bounds.size();
        DEBUG_ASSERT(uv.x >= 0.0 && uv.x <= 1.0);
        DEBUG_ASSERT(uv.y >= 0.0 && uv.y <= 1.0);
    }
}
