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

#include "io/image.h"

Expected<void> image::save_image_as_png(const radix::Raster<glm::u8vec3>& input_image, const std::string& path)
{
    if (input_image.width() == 0 || input_image.height() == 0) {
        return Error::fail(Error::Code::InvalidInput, "cannot write an empty tile builder image");
    }
    try {
        // The legacy tile builder stores its first row at the bottom.
        auto flipped = input_image;
        for (unsigned y = 0; y < input_image.height(); ++y) {
            std::copy_n(input_image.data() + std::size_t(y) * input_image.width(),
                input_image.width(),
                flipped.data() + std::size_t(input_image.height() - 1 - y) * input_image.width());
        }
        return io::image::write(flipped, path, { .overwrite = true, .make_dirs = false });
    } catch (const std::bad_alloc&) {
        return Error::fail(Error::Code::ResourceExhausted, "flip tile builder image");
    }
}
