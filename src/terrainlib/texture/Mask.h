#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>

namespace texture {

// A boolean mask over an image.
class Mask {
public:
    explicit Mask(const glm::uvec2 size)
        : _mask(size.y, size.x, CV_8UC1, cv::Scalar::all(0)) {
    }

    bool get(const int x, const int y) const {
        return this->_mask.at<uint8_t>(y, x) != 0;
    }

    void set(const int x, const int y, const bool value = true) {
        this->_mask.at<uint8_t>(y, x) = value ? 255 : 0;
    }

    glm::uvec2 size() const {
        return {this->_mask.cols, this->_mask.rows};
    }

    uint32_t count() const {
        return cv::countNonZero(this->_mask);
    }

    Mask clone() const {
        Mask copy(this->size());
        this->_mask.copyTo(copy._mask);
        return copy;
    }

private:
    cv::Mat _mask;
};

} // namespace texture