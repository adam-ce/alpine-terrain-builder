#include <cstdint>
#include <span>
#include <vector>
#include <algorithm>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>
#include <radix/geometry.h>
#include <libassert/assert.hpp>

#include "log.h"
#include "mesh/texture_trim.h"
#include "range_utils.h"

TrimResult trim_texture(const cv::Mat &texture, const std::span<const glm::dvec2> uvs, const uint32_t padding) {
    TrimResult result;
    result.uvs.resize(uvs.size());
    const TextureTrim trim = compute_texture_trim(texture, uvs, padding);
    result.texture = trim.texture;
    trim.uv_remap.remap_uvs(uvs, result.uvs);
    return result;
}

void trim_texture_inplace(cv::Mat &texture, const std::span<glm::dvec2> uvs, const uint32_t padding) {
    const TextureTrim trim = compute_texture_trim(texture, uvs, padding);
    texture = trim.texture;
    trim.uv_remap.remap_uvs_inplace(uvs);
}

namespace detail {
inline TextureTrim identity_trim(const cv::Mat& texture) {
    return TextureTrim{.texture = texture, .uv_remap = UvRemap::identity()};
}

bool is_empty(const radix::geometry::Aabb2i &bounds) {
    return bounds.min.x >= bounds.max.x || bounds.min.y >= bounds.max.y;
}
}

TextureTrim compute_texture_trim(const cv::Mat &texture, const std::span<const glm::dvec2> uvs, const uint32_t padding) {
    const uint32_t uv_count = uvs.size();
    if (uv_count == 0) {
        return detail::identity_trim(texture);
    }

    const radix::geometry::Aabb2d uv_bounds = radix::geometry::find_bounds(std::span<const glm::dvec2>(uvs));
    return compute_texture_trim(texture, uv_bounds, padding);
}

TextureTrim compute_texture_trim(const cv::Mat &texture, const radix::geometry::Aabb2d &uv_bounds, const uint32_t padding) {
    if (texture.empty()) {
        return detail::identity_trim(texture);
    }

    // Calculate the bounding box of the UVs in pixel space
    const glm::uvec2 texture_size(texture.cols, texture.rows);
    const radix::geometry::Aabb2i pixel_bounds = {
        glm::floor(uv_bounds.min * glm::dvec2(texture_size)),
        glm::ceil(uv_bounds.max * glm::dvec2(texture_size))};
    const radix::geometry::Aabb2i padded_pixel_bounds = {
        pixel_bounds.min - glm::ivec2(padding),
        pixel_bounds.max + glm::ivec2(padding)};
    const radix::geometry::Aabb2i full_bounds(glm::ivec2(0), glm::ivec2(texture_size));
    const radix::geometry::Aabb2i clamped_pixel_bounds(radix::geometry::intersection(padded_pixel_bounds, full_bounds));

    // Return identity trim if uv bounds are empty or full texture
    if (detail::is_empty(clamped_pixel_bounds) || clamped_pixel_bounds == full_bounds) {
        return detail::identity_trim(texture);
    }

    // Crop the texture to the bounding box
    const cv::Rect roi(
        clamped_pixel_bounds.min.x,
        clamped_pixel_bounds.min.y,
        clamped_pixel_bounds.width(),
        clamped_pixel_bounds.height());
    cv::Mat cropped_texture = texture(roi);
    
    // Clone the cropped texture if size difference is large to avoid keeping a large texture in memory just for a small cropped area
    constexpr float AreaThreshold = 0.125f;
    const float cropped_area = roi.width * roi.height;
    const float original_area = texture_size.x * texture_size.y;
    if (cropped_area < original_area * AreaThreshold) {
        cropped_texture = cropped_texture.clone();
    }

    // Update the UVs to match the cropped texture
    const glm::uvec2 cropped_texture_size(cropped_texture.cols, cropped_texture.rows);
    DEBUG_ASSERT(cropped_texture_size != texture_size);
    LOG_TRACE("Trimmed texture from {}x{} to {}x{}", texture_size.x, texture_size.y, cropped_texture_size.x, cropped_texture_size.y);
    const glm::dvec2 offset = glm::dvec2(clamped_pixel_bounds.min) / glm::dvec2(texture_size);
    const glm::dvec2 scale = glm::dvec2(clamped_pixel_bounds.size()) / glm::dvec2(texture_size);
    
    const UvRemap uv_remap{.offset = offset, .scale = scale};
    return TextureTrim{.texture = cropped_texture, .uv_remap = uv_remap};
}
