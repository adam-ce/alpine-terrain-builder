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