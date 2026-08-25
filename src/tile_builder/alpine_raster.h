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

#ifndef ALPINERASTERGENERATOR_H
#define ALPINERASTERGENERATOR_H

#include <string>

#include <glm/glm.hpp>
#include <vector>

#include <radix/raster.h>
#include "ParallelTileGenerator.h"
#include <radix/tile.h>
#include "ctb/Grid.hpp"

namespace alpine_raster {

class TileWriter : public ParallelTileWriterInterface {
public:
    TileWriter(radix::tile::Border border)
        : ParallelTileWriterInterface(border, "png")
    {
    }
    void write(const std::string& base_path, const radix::tile::Descriptor& tile, const radix::Raster<float>& heights) const override;
};
[[nodiscard]] ParallelTileGenerator make_generator(
    const std::string& input_data_path,
    const std::string& output_data_path,
    ctb::Grid::Srs srs,
    radix::tile::Border border,
    unsigned grid_resolution = 256);
};

#endif // ALPINERASTERGENERATOR_H
