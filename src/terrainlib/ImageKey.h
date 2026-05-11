#pragma once 

#include <cstddef>
#include <cstdint>

#include <opencv2/core.hpp>

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

    bool operator<=>(const ImageKey &other) const = default;
};
