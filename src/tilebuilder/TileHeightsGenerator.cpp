/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 madam
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

#include "TileHeightsGenerator.h"

#include <utility>

#include "Dataset.h"
#include "DatasetReader.h"
#include "TopDownTiler.h"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"
#include "depth_first_tile_traverser.h"
#include <radix/TileHeights.h>

TileHeightsGenerator::TileHeightsGenerator(std::string input_data_path, ctb::Grid::Srs srs, radix::tile::Scheme scheme, radix::tile::Border border, std::filesystem::path output_path)
    : m_input_data_path(std::move(input_data_path))
    , m_srs(srs)
    , m_scheme(scheme)
    , m_border(border)
    , m_output_path(std::move(output_path))
{
}

void TileHeightsGenerator::run(unsigned max_zoom_level) const
{
    struct MinMaxData {
        radix::tile::Id tile_id;
        std::pair<float, float> min_max = std::make_pair(0, 9000);
    };

    const auto dataset = std::make_shared<Dataset>(std::filesystem::path(m_input_data_path));

    ctb::Grid grid = ctb::GlobalGeodetic(64);
    if (m_srs == ctb::Grid::Srs::SphericalMercator)
        grid = ctb::GlobalMercator(64);
    const auto bounds = dataset->bounds(grid.getSRS());
    const auto tile_reader = DatasetReader(dataset, grid.getSRS(), 1, false);
    const auto tiler = TopDownTiler(grid, bounds, m_border, m_scheme);
    auto tile_heights = radix::TileHeights();

    const auto read_function = [&](const radix::tile::Descriptor& tile) -> MinMaxData {
        const auto tile_data = tile_reader.readWithOverviews(tile.srsBounds, tile.tileSize, tile.tileSize);
        auto [min, max] = std::ranges::minmax(tile_data);
        tile_heights.emplace(tile.id, std::make_pair(min, max));
        return { tile.id, std::make_pair(min, max) };
    };

    std::vector<std::vector<std::pair<float, float>>> aggregate_calls;
    const auto aggregate_function = [&](const std::vector<MinMaxData>& data) -> MinMaxData {
        const auto new_id = data.front().tile_id.parent();
        auto min_max = data.front().min_max;
        for (const auto& d : data) {
            min_max.first = std::min(min_max.first, d.min_max.first);
            min_max.second = std::max(min_max.second, d.min_max.second);
        }
        tile_heights.emplace(new_id, min_max);
        return { new_id, min_max };
    };


    traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, { 0, { 0, 0 }, m_scheme }, max_zoom_level);
    if (m_srs == ctb::Grid::Srs::WGS84) {
        // two root tiles
        traverse_depth_first_and_aggregate(tiler, read_function, aggregate_function, { 0, { 1, 0 }, m_scheme }, max_zoom_level);
    }
    tile_heights.write_to(m_output_path);

}
