/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Martin Braunsperger
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

#include <cstddef>
#include <cstdint>

#include <opencv2/core.hpp>

#include "hash_utils.h"

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

    auto operator<=>(const ImageKey &other) const = default;
};

namespace std {
template <>
struct hash<ImageKey> {
    size_t operator()(const ImageKey &key) const noexcept {
        return ::hash::combine(key.data, key.rows, key.cols, key.step, key.type);
    }
};
} // namespace std
