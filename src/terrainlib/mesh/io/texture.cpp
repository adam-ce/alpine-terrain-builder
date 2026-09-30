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

#include "mesh/io/texture.h"

namespace mesh::io {

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer) {
    cv::Mat raw_data = cv::Mat(1, buffer.size(), CV_8UC1, const_cast<uint8_t *>(buffer.data()));
    cv::Mat mat = cv::imdecode(raw_data, cv::IMREAD_UNCHANGED);
    mat.convertTo(mat, CV_8UC3);
    return mat;
}

void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string& extension) {
    cv::Mat converted;
    image.convertTo(converted, CV_8UC3);
    cv::imencode(extension, converted, buffer);
}
void write_texture_to_encoded_buffer(const ImageAndExt &item, std::vector<uint8_t> &buffer) {
    write_texture_to_encoded_buffer(item.image, buffer, item.ext);
}
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string &extension) {
    std::vector<uint8_t> buffer;
    write_texture_to_encoded_buffer(image, buffer, extension);
    return buffer;
}
std::vector<uint8_t> write_texture_to_encoded_buffer(const ImageAndExt &item) {
    return write_texture_to_encoded_buffer(item.image, item.ext);
}

}
