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

#include <opencv2/core.hpp>

#include "texture/Mask.h"

namespace texture {

// Grows the covered region by radius texels, each new texel taking the mean of its covered neighbours.
void dilate_colors_inplace(cv::Mat &image, Mask &coverage, const uint32_t radius);

// Grows the covered region by radius texels, each new texel taking the mean of its covered neighbours.
cv::Mat dilate_colors(const cv::Mat &image, const Mask &coverage, const uint32_t radius);

} // namespace texture