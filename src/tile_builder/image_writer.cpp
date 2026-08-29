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

#include "io/conversion.h"

#include <opencv2/opencv.hpp>

Expected<void> image::save_image_as_png(const radix::Raster<glm::u8vec3>& input_image, const std::string& path)
{
    auto converted = io::conversion::to_mat(input_image);
    if (!converted) {
        return Error::propagate(std::move(converted), "convert raster for PNG output");
    }

    cv::Mat flipped;
    cv::Mat image;
    try {
        cv::flip(*converted, flipped, 0);
        cv::cvtColor(flipped, image, cv::COLOR_RGB2BGR);
    } catch (const cv::Exception& error) {
        return Error::fail(Error::Code::Internal, "prepare PNG image: " + error.msg);
    }

    try {
        if (!cv::imwrite(path, image)) {
            return Error::fail(Error::Code::Io, "write PNG image to \"" + path + "\"");
        }
    } catch (const cv::Exception& error) {
        return Error::fail(Error::Code::Io, "write PNG image to \"" + path + "\": " + error.msg);
    }
    return {};
}
