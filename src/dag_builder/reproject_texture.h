#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/common.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

class TextureReprojector {
public:
    // Construct with size and optional type
    TextureReprojector(const glm::uvec2 size, const int32_t type = CV_32FC3)
        : TextureReprojector(cv::Mat::zeros(size.y, size.x, type)) {}

    // Construct with an existing texture to draw onto
    TextureReprojector(cv::Mat texture)
        : target_image(std::move(texture)) {
        this->weight_image = cv::Mat::ones(this->target_image.size(), CV_32FC1);
        this->target_type = this->target_image.type();
        this->target_image.convertTo(this->target_image, CV_32FC3);
    }

    // Warps a single triangle from the source image onto the internal target texture
    void add_triangle(const cv::Mat &source_image,
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

        // Read source region from source image
        const cv::Mat source_view = source_image(source_rect);

        // Ensure floating point for warping if needed, or maintain original type
        const int32_t original_type = source_image_cropped.type();
        cv::Mat source_image_cropped;
        source_view.convertTo(source_image_cropped, CV_32FC3);

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

    // Returns the finished texture
    cv::Mat finish(bool allow_continue = false) {
        ASSERT(!this->target_image.empty());

        cv::Mat output_image;
        if (allow_continue) {
            output_image = cv::Mat::zeros(this->target_image.size, CV_32FC3);
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
        output_image.convertTo(output_image, this->input_type);
        return std::move(output_image);
    }

private:
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
};

// --- Original functions using the new class ---

inline void reproject_texture(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> old_uvs,
    const std::span<const glm::dvec2> new_uvs,
    const cv::Mat &old_texture,
    cv::Mat &new_texture) {

    TextureReprojector reprojector(new_texture);
    const glm::dvec2 old_texture_size(old_texture.cols, old_texture.rows);
    const glm::dvec2 new_texture_size(new_texture.cols, new_texture.rows);

    for (const glm::uvec3 &triangle : triangles) {
        std::array<cv::Point2f, 3> old_uv_triangle;
        std::array<cv::Point2f, 3> new_uv_triangle;
        for (uint8_t k = 0; k < 3; k++) {
            const uint32_t vertex_index = triangle[k];
            const glm::dvec2 old_uv = old_uvs[vertex_index] * old_texture_size;
            const glm::dvec2 new_uv = new_uvs[vertex_index] * new_texture_size;
            old_uv_triangle[k] = cv::Point2f(static_cast<float>(old_uv.x), static_cast<float>(old_uv.y));
            new_uv_triangle[k] = cv::Point2f(static_cast<float>(new_uv.x), static_cast<float>(new_uv.y));
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
