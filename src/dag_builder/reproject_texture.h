#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "glm_utils.h"

namespace detail {
struct ImageKey {
    const uint8_t *data = nullptr;
    int32_t rows = 0;
    int32_t cols = 0;
    size_t step = 0;
    int32_t type = 0;

    ImageKey() = default;
    ImageKey(const cv::Mat &mat) {
        this->data = mat.data;
        this->rows = mat.rows;
        this->cols = mat.cols;
        this->step = mat.step;
        this->type = mat.type();
    }

    bool operator==(const ImageKey &other) const = default;
};
}
class TextureReprojector {
public:
    // Construct with size and optional type
    TextureReprojector(const glm::uvec2 size, const int32_t type = CV_32FC3)
        : TextureReprojector(cv::Mat::zeros(size.y, size.x, type)) {}

    // Construct with an existing texture to draw onto
    TextureReprojector(cv::Mat texture)
        : target_image(std::move(texture)) {
        this->weight_image = cv::Mat::zeros(this->target_image.size(), CV_32FC1);
        this->target_type = this->target_image.type();
        this->target_image.convertTo(this->target_image, CV_32FC3);
        this->cached_source_image = cv::Mat();
    }

    // Warps a single triangle from the source image onto the internal target texture.
    void add_scaled_triangle(const cv::Mat &source_image,
                      const std::array<cv::Point2f, 3> source_triangle,
                      const std::array<cv::Point2f, 3> target_triangle) {
        ASSERT(!this->target_image.empty());

        // Find bounding rectangle for each triangle
        const cv::Rect source_rect = clamp_rect_to_mat_bounds(cv::boundingRect(source_triangle), source_image);
        const cv::Rect target_rect = clamp_rect_to_mat_bounds(cv::boundingRect(target_triangle), target_image);

        if (source_rect.width <= 0 || source_rect.height <= 0 ||
            target_rect.width <= 0 || target_rect.height <= 0) {
            return;
        }

        // Relativize triangles to bounds
        std::array<cv::Point2f, 3> source_triangle_cropped;
        std::array<cv::Point2f, 3> target_triangle_cropped;
        for (uint8_t i = 0; i < 3; i++) {
            source_triangle_cropped[i] = cv::Point2f(source_triangle[i].x - source_rect.x, source_triangle[i].y - source_rect.y);
            target_triangle_cropped[i] = cv::Point2f(target_triangle[i].x - target_rect.x, target_triangle[i].y - target_rect.y);
        }

        // Convert points to int triangles as fillConvexPoly needs a vector of Point and not Point2f
        std::array<cv::Point2i, 3> target_triangle_cropped_int;
        for (uint8_t i = 0; i < 3; i++) {
            target_triangle_cropped_int[i] = cv::Point2i(static_cast<int32_t>(target_triangle[i].x - target_rect.x),
                                                         static_cast<int32_t>(target_triangle[i].y - target_rect.y));
        }

        // Convert source image into floating point and cache
        // This assumes that this method is often called with the same texture sequentially and that
        // most of the texture is used.
        if (this->cached_source_image_key != detail::ImageKey(source_image)) {
            source_image.convertTo(this->cached_source_image, CV_32FC3);
            this->cached_source_image_key = detail::ImageKey(source_image);
        }

        // Read source region from source image
        const cv::Mat source_image_cropped = this->cached_source_image(source_rect);

        // Given a pair of triangles, find the affine transform
        const cv::Mat warp_transform = cv::getAffineTransform(source_triangle_cropped, target_triangle_cropped);

        // Apply the affine transform just found to the source image
        cv::Mat target_image_cropped = cv::Mat::zeros(target_rect.height, target_rect.width, CV_32FC3);
        cv::warpAffine(source_image_cropped, target_image_cropped, warp_transform, target_image_cropped.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

        // Get mask by filling triangle
        cv::Mat mask = cv::Mat::zeros(target_rect.height, target_rect.width, CV_32FC1);
        cv::fillConvexPoly(mask, target_triangle_cropped_int, cv::Scalar(1.0), 16, 0);

        // Prepare 3-channel mask for color accumulation
        cv::Mat mask_color;
        cv::merge(std::vector<cv::Mat>{mask, mask, mask}, mask_color);
        
        // Isolate the triangle area in the warped patch
        cv::multiply(target_image_cropped, mask_color, target_image_cropped);
        
        // Additively blend color and weight into the accumulation buffers
        this->target_image(target_rect) += target_image_cropped;
        this->weight_image(target_rect) += mask;
    }
    // Warps a single triangle from the source image onto the internal target texture.
    void add_scaled_triangle(const cv::Mat &source_image,
                      const std::array<glm::dvec2, 3> source_triangle,
                      const std::array<glm::dvec2, 3> target_triangle) {
        std::array<cv::Point2f, 3> cv_source_triangle;
        std::array<cv::Point2f, 3> cv_target_triangle;

        for (uint8_t k = 0; k < 3; k++) {
            cv_source_triangle[k] = glm_to_cv(source_triangle[k]);
            cv_target_triangle[k] = glm_to_cv(target_triangle[k]);
        }

        this->add_scaled_triangle(source_image, cv_source_triangle, cv_target_triangle);
    }

    // Warps a single triangle from the source image onto the internal target texture.
    void add_triangle(const cv::Mat &source_image,
                      const std::array<glm::dvec2, 3> &source_uv_triangle,
                      const std::array<glm::dvec2, 3> &target_uv_triangle) {
        const glm::dvec2 source_texture_size(source_image.cols, source_image.rows);
        const glm::dvec2 target_texture_size(this->target_image.cols, this->target_image.rows);

        std::array<cv::Point2f, 3> source_pixel_triangle;
        std::array<cv::Point2f, 3> target_pixel_triangle;

        for (uint8_t k = 0; k < 3; k++) {
            const glm::dvec2 source_pixel = source_uv_triangle[k] * source_texture_size;
            const glm::dvec2 target_pixel = target_uv_triangle[k] * target_texture_size;
            source_pixel_triangle[k] = glm_to_cv(source_pixel);
            target_pixel_triangle[k] = glm_to_cv(target_pixel);
        }

        this->add_scaled_triangle(source_image, source_pixel_triangle, target_pixel_triangle);
    }
    // Warps a single triangle from the source image onto the internal target texture.
    void add_triangle(const cv::Mat &source_image,
                      const glm::uvec3 &source_triangle,
                      const std::span<const glm::dvec2> source_uvs,
                      const glm::uvec3 &target_triangle,
                      const std::span<const glm::dvec2> target_uvs) {
        std::array<glm::dvec2, 3> source_uv_triangle;
        std::array<glm::dvec2, 3> target_uv_triangle;

        for (uint8_t k = 0; k < 3; k++) {
            source_uv_triangle[k] = source_uvs[source_triangle[k]];
            target_uv_triangle[k] = target_uvs[target_triangle[k]];
        }

        this->add_triangle(source_image, source_uv_triangle, target_uv_triangle);
    }
    // Warps a single triangle from the source image onto the internal target texture.
    void add_triangle(const cv::Mat &source_image,
                      const glm::uvec3 &triangle,
                      const std::span<const glm::dvec2> source_uvs,
                      const std::span<const glm::dvec2> target_uvs) {
        this->add_triangle(source_image, triangle, source_uvs, triangle, target_uvs);
    }
    // Warps triangles from the source image onto the internal target texture.
    void add_triangles(const cv::Mat &source_image,
                      const std::span<const glm::uvec3> triangles,
                      const std::span<const glm::dvec2> source_uvs,
                      const std::span<const glm::dvec2> target_uvs) {
        for (const glm::uvec3 &triangle : triangles) {
            this->add_triangle(source_image, triangle, source_uvs, target_uvs);
        }
    }

    // Returns the finished texture
    cv::Mat finish(bool allow_continue = false) {
        ASSERT(!this->target_image.empty());

        cv::Mat output_image;
        if (allow_continue) {
            output_image = cv::Mat::zeros(this->target_image.size(), this->target_image.type());
        } else {
            output_image = this->target_image;
        }

        cv::Mat weight_3c;
        cv::merge(std::vector<cv::Mat>{this->weight_image, this->weight_image, this->weight_image}, weight_3c);
        
        // Normalize colors by dividing by the number of contributing triangles
        // Using a small epsilon to avoid division by zero on empty pixels
        cv::divide(this->target_image, weight_3c + 1e-6, output_image);

        if (!allow_continue) {
            this->target_image.release();
            this->weight_image.release();
        }

        // Convert back to the original input type
        output_image.convertTo(output_image, this->target_type);
        return output_image;
    }

private:
    detail::ImageKey cached_source_image_key;
    cv::Mat cached_source_image;
    cv::Mat target_image;
    cv::Mat weight_image;
    int32_t target_type;

    template <typename T>
    static cv::Rect_<T> clamp_rect_to_mat_bounds(const cv::Rect_<T> &rect, const cv::Mat &mat) {
        const T x = std::max(rect.x, static_cast<T>(0));
        const T y = std::max(rect.y, static_cast<T>(0));
        const T width = std::max(std::min(rect.width, static_cast<T>(mat.cols) - x), static_cast<T>(0));
        const T height = std::max(std::min(rect.height, static_cast<T>(mat.rows) - y), static_cast<T>(0));
        return cv::Rect_<T>(x, y, width, height);
    }
    static inline cv::Point2f glm_to_cv(const glm::dvec2 &vec) {
        return cv::Point2f(static_cast<float>(vec.x), static_cast<float>(vec.y));
    };
};

inline void reproject_texture(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> old_uvs,
    const std::span<const glm::dvec2> new_uvs,
    const cv::Mat &old_texture,
    cv::Mat &new_texture) {
    TextureReprojector reprojector(new_texture);
    std::array<glm::dvec2, 3> old_uv_triangle;
    std::array<glm::dvec2, 3> new_uv_triangle;
    for (const glm::uvec3 &triangle : triangles) {
        for (const auto [k, vertex_index] : enumerate(triangle)) {
            old_uv_triangle[k] = old_uvs[vertex_index];
            new_uv_triangle[k] = new_uvs[vertex_index];
        }
        reprojector.add_triangle(old_texture, old_uv_triangle, new_uv_triangle);
    }
    new_texture = reprojector.finish();
}

inline cv::Mat reproject_texture(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> old_uvs,
    const std::span<const glm::dvec2> new_uvs,
    const cv::Mat &old_texture,
    const glm::uvec2 new_texture_size) {
    cv::Mat new_texture = cv::Mat::zeros(new_texture_size.y, new_texture_size.x, old_texture.type());
    reproject_texture(triangles, old_uvs, new_uvs, old_texture, new_texture);
    return new_texture;
}
