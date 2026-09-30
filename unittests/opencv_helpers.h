/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
