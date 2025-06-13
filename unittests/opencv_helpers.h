#pragma once

#include <opencv2/opencv.hpp>

// modified from https://stackoverflow.com/a/32440830/6304917
inline bool mat_equals(const cv::Mat mat1, const cv::Mat mat2) {
    if (mat1.dims != mat2.dims ||
        mat1.size != mat2.size ||
        mat1.elemSize() != mat2.elemSize()) {
        return false;
    }

    if (mat1.isContinuous() && mat2.isContinuous()) {
        return std::memcmp(mat1.ptr(), mat2.ptr(), mat1.total() * mat1.elemSize()) == 0;
    } else {
        const cv::Mat *arrays[] = {&mat1, &mat2, 0};
        uchar *ptrs[2];
        cv::NAryMatIterator it(arrays, ptrs, 2);
        for (unsigned int p = 0; p < it.nplanes; p++, ++it) {
            if (memcmp(it.ptrs[0], it.ptrs[1], it.size * mat1.elemSize()) != 0) {
                return false;
            }
        }

        return true;
    }
}
