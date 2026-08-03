#pragma once

#include <glm/common.hpp>
#include <opencv2/opencv.hpp>

inline size_t mat_byte_size(const cv::Mat &mat) {
    return mat.total() * mat.elemSize();
}

inline glm::uvec2 get_texture_size(const cv::Mat &texture) {
    return {texture.cols, texture.rows};
}

inline void rescale_texture(const cv::Mat &source, cv::Mat &destination, const glm::uvec2 new_size) {
    cv::resize(source, destination, cv::Size(new_size.x, new_size.y), 0, 0);
}

inline cv::Mat rescale_texture(const cv::Mat &source, const glm::uvec2 new_size) {
    cv::Mat destination;
    rescale_texture(source, destination, new_size);
    return destination;
}

inline void rescale_texture_inplace(cv::Mat &texture, const glm::uvec2 new_size) {
    rescale_texture(texture, texture, new_size);
}
