#pragma once

#include <stdexcept>

#include <glm/common.hpp>
#include <opencv2/opencv.hpp>

// Calls the visitor with the depth's channel type.
template <typename Visitor>
decltype(auto) visit_by_depth(const int depth, Visitor &&visitor) {
    switch (depth) {
    case CV_8U:
        return visitor.template operator()<uchar>();
    case CV_8S:
        return visitor.template operator()<schar>();
    case CV_16U:
        return visitor.template operator()<ushort>();
    case CV_16S:
        return visitor.template operator()<short>();
    case CV_32S:
        return visitor.template operator()<int>();
    case CV_32F:
        return visitor.template operator()<float>();
    case CV_64F:
        return visitor.template operator()<double>();
    default:
        throw std::invalid_argument("visit_by_depth: unsupported depth");
    }
}

// Calls the visitor with the channel count.
template <typename Visitor>
decltype(auto) visit_by_channels(const int channels, Visitor &&visitor) {
    switch (channels) {
    case 1:
        return visitor.template operator()<1>();
    case 2:
        return visitor.template operator()<2>();
    case 3:
        return visitor.template operator()<3>();
    case 4:
        return visitor.template operator()<4>();
    default:
        throw std::invalid_argument("visit_by_channels: beyond four channels is not supported");
    }
}

// Calls the visitor with both the channel count and type.
template <typename Visitor>
decltype(auto) visit_by_depth_and_channels(const int type, Visitor &&visitor) {
    return visit_by_channels(CV_MAT_CN(type), [&]<int n_channels>() -> decltype(auto) {
        return visit_by_depth(CV_MAT_DEPTH(type), [&]<typename Channel>() -> decltype(auto) {
            return visitor.template operator()<n_channels, Channel>();
        });
    });
}

inline size_t mat_byte_size(const cv::Mat &mat) {
    return mat.total() * mat.elemSize();
}

inline glm::uvec2 get_texture_size(const cv::Mat &texture) {
    return {texture.cols, texture.rows};
}

inline void rescale_texture(const cv::Mat &source, cv::Mat &destination, const glm::uvec2 new_size) {
    const glm::uvec2 source_size = get_texture_size(source);
    const bool minifying = glm::all(glm::lessThanEqual(new_size, source_size));
    const int interpolation = minifying ? cv::INTER_AREA : cv::INTER_CUBIC;
    cv::resize(source, destination, cv::Size(new_size.x, new_size.y), 0, 0, interpolation);
}

inline cv::Mat rescale_texture(const cv::Mat &source, const glm::uvec2 new_size) {
    cv::Mat destination;
    rescale_texture(source, destination, new_size);
    return destination;
}

inline void rescale_texture_inplace(cv::Mat &texture, const glm::uvec2 new_size) {
    rescale_texture(texture, texture, new_size);
}
