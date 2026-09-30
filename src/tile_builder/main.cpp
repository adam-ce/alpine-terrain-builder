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

#include <filesystem>
#include <fstream>

#include <glm/glm.hpp>

#include "ParallelTiler.h"
#include "TileHeightsGenerator.h"
#include "alpine_raster.h"
#include "ctb/Grid.hpp"

int main()
{
    //  const std::string input_raster = "/home/madam/rajaton/raw/Oe_2020/OeRect_01m_gs_31287.img";
    //  const std::string output_path = "/home/madam/rajaton/tiles/atb_terrain/";
    ////  const auto generator = alpine_raster::make_generator("./test_tiles/", "/home/madam/valtava/raw/Oe_2020/OeRect_01m_gs_31287.img", ctb::Grid::Srs::SphericalMercator, radix::tile::Border::No);
    ////  generator.process({16, 16});
    //  generator.process({0, 5}, true, true);
    //  generator.process({6, 16}, true, false);

        const std::string input_raster = "/home/madam/valtava/raw/Oe_2020/OeRect_01m_gs_31287.img";
//    const std::string input_raster = "/home/madam/valtava/raw/vienna/innenstadt_gs_1m_mgi.tif";
    const std::string output_path = "/home/madam/valtava/tiles/alpine_png2";
    //  const auto generator = alpine_raster::make_generator("./test_tiles/", "/home/madam/valtava/raw/Oe_2020/OeRect_01m_gs_31287.img", ctb::Grid::Srs::SphericalMercator, radix::tile::Border::No);
    //  generator.process({16, 16});
    const auto generator = alpine_raster::make_generator(input_raster, output_path, ctb::Grid::Srs::SphericalMercator, radix::tile::Border::Yes, 64);
    //    generator.process({0, 5}, true, true);
    generator.process({ 15, 16 }, true, false);

    //     const auto json = layer_json_writer::process(metadata);

    //// generate height data (min and max) for tiles up to level 13
//    const auto base_path = std::filesystem::path(output_path);
//    constexpr auto file_name = "height_data.atb";
//    const auto generator = TileHeightsGenerator(input_raster, ctb::Grid::Srs::SphericalMercator, radix::tile::Border::Yes, base_path / file_name);
//    generator.run(13);

    return 0;
}
