#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/texture_trim.h"
#include "range_utils.h"

namespace {
bool is_empty(const radix::geometry::Aabb2i &bounds) {
    return bounds.min.x >= bounds.max.x || bounds.min.y >= bounds.max.y;
}

void trim_texture_impl(
    const cv::Mat &input_texture,
    cv::Mat &output_texture,
    const std::span<const glm::dvec2> input_uvs,
    std::span<glm::dvec2> output_uvs,
    const uint32_t padding) {
    const uint32_t uv_count = input_uvs.size();
    DEBUG_ASSERT(output_uvs.size() >= uv_count);

    // Calculate the bounding box of the UVs in pixel space
    const radix::geometry::Aabb2d uv_bounds = radix::geometry::find_bounds(std::span<const glm::dvec2>(input_uvs));
    const glm::uvec2 texture_size(input_texture.cols, input_texture.rows);
    const radix::geometry::Aabb2i pixel_bounds = {
        glm::floor(uv_bounds.min * glm::dvec2(texture_size)),
        glm::ceil(uv_bounds.max * glm::dvec2(texture_size))};
    const radix::geometry::Aabb2i padded_pixel_bounds = {
        pixel_bounds.min - glm::ivec2(padding),
        pixel_bounds.max + glm::ivec2(padding)};
    const radix::geometry::Aabb2i full_bounds(glm::ivec2(0), glm::ivec2(texture_size));
    const radix::geometry::Aabb2i clamped_pixel_bounds(radix::geometry::intersection(padded_pixel_bounds, full_bounds));

    // Return empty texture if uv bounds are empty
    if (is_empty(clamped_pixel_bounds)) {
        output_texture = cv::Mat();
        for (glm::dvec2& uv : output_uvs) {
            uv = glm::dvec2(0);
        }
        return;
    }

    // Skip if no-op
    if (&input_texture == &output_texture && clamped_pixel_bounds == full_bounds) {
        return;
    }

    // Crop the texture to the bounding box
    const cv::Rect roi(
        clamped_pixel_bounds.min.x,
        clamped_pixel_bounds.min.y,
        clamped_pixel_bounds.width(),
        clamped_pixel_bounds.height());
    output_texture = input_texture(roi).clone();

    // Update the UVs to match the cropped texture
    const glm::uvec2 cropped_texture_size(output_texture.cols, output_texture.rows);
    LOG_TRACE("Trimmed texture from {}x{} to {}x{}", texture_size.x, texture_size.y, cropped_texture_size.x, cropped_texture_size.y);
    const glm::dvec2 offset = glm::dvec2(clamped_pixel_bounds.min) / glm::dvec2(texture_size);
    const glm::dvec2 scale = glm::dvec2(clamped_pixel_bounds.size()) / glm::dvec2(texture_size);
    for (const size_t i : range(uv_count)) {
        output_uvs[i] = (input_uvs[i] - offset) / scale;
        DEBUG_ASSERT(output_uvs[i].x >= 0.0 && output_uvs[i].x <= 1.0);
        DEBUG_ASSERT(output_uvs[i].y >= 0.0 && output_uvs[i].y <= 1.0);
    }
}
}

TrimResult trim_texture(const cv::Mat &texture, const std::span<const glm::dvec2> uvs, const uint32_t padding) {
    TrimResult result;
    result.uvs.resize(uvs.size());
    trim_texture_impl(texture, result.texture, uvs, result.uvs, padding);
    return result;
}

void trim_texture_inplace(cv::Mat &texture, const std::span<glm::dvec2> uvs, const uint32_t padding) {
    trim_texture_impl(texture, texture, uvs, uvs, padding);
}
