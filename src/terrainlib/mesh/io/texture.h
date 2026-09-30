/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adam Celarek-Litofcenko
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

#include <string_view>
#include <vector>
#include <span>

#include <opencv2/opencv.hpp>

namespace mesh::io {

struct ImageAndExt {
    cv::Mat image;
    std::string ext;
};

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer);
void write_texture_to_encoded_buffer(const ImageAndExt& item, std::vector<uint8_t> &buffer);
void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string &extension);
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string &extension);
std::vector<uint8_t> write_texture_to_encoded_buffer(const ImageAndExt &item);

}
