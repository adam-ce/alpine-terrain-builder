/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 * Copyright (C) 2022 Adam Celarek <family name at cg tuwien ac at>
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

#include "Image.h"
#include "init.h"
#include <FreeImage.h>
#include <cstdlib>

void image::saveImageAsPng(const Image<glm::u8vec3>& image, const std::string& path)
{
    tntn::initialize_freeimage_once();

    FIBITMAP* free_bitmap = FreeImage_Allocate(int(image.width()), int(image.height()), 24);
    // free image has line 0 at the bottom
    for (unsigned free_row = 0; free_row < image.height(); ++free_row) {
        unsigned alpine_row = image.height() - free_row - 1;
        assert(alpine_row < image.height());
        for (unsigned col = 0; col < image.width(); ++col) {
            RGBQUAD colour;
            colour.rgbRed = image.pixel(alpine_row, col).x;
            colour.rgbGreen = image.pixel(alpine_row, col).y;
            colour.rgbBlue = image.pixel(alpine_row, col).z;
            FreeImage_SetPixelColor(free_bitmap, col, free_row, &colour);
        }
    }
    FreeImage_Save(FIF_PNG, free_bitmap, path.c_str(), PNG_Z_BEST_COMPRESSION);
    FreeImage_Unload(free_bitmap);
    //  FreeImage_DeInitialise();
}
