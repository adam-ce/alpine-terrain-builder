/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
 * Copyright (C) 2025 Martin Braunsperger <e11909911@student.tuwien.ac.at>
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

#include "image_writer.h"

#include <opencv2/opencv.hpp>
#include <stdexcept>

void image::saveImageAsPng(const radix::Raster<glm::u8vec3>& input_image, const std::string& path)
{
    const int width = static_cast<int>(input_image.width());
    const int height = static_cast<int>(input_image.height());

    cv::Mat image(height, width, CV_8UC3);

    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const auto& pixel = input_image.pixel({ static_cast<unsigned>(column), static_cast<unsigned>(height - row - 1) });
            image.at<cv::Vec3b>(row, column) = cv::Vec3b(pixel.z, pixel.y, pixel.x);
        }
    }

    try {
        if (!cv::imwrite(path, image))
            throw std::runtime_error("Failed to write PNG image to " + path);
    } catch (const cv::Exception& error) {
        throw std::runtime_error("Failed to write PNG image to " + path + ": " + error.what());
    }
}
