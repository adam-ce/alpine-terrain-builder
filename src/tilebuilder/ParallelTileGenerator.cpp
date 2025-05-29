/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
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

#include "ParallelTileGenerator.h"
#include "ProgressIndicator.h"

#include <execution>
#include <filesystem>

#include <fmt/core.h>

#include "Dataset.h"
#include "ctb/GlobalGeodetic.hpp"
#include "ctb/GlobalMercator.hpp"

#include <DatasetReader.h>
#include <thread>

ParallelTileGenerator::ParallelTileGenerator(const std::string& input_data_path, const ctb::Grid& grid, const ParallelTiler& tiler, std::unique_ptr<ParallelTileWriterInterface> tile_writer, const std::string& output_data_path)
    : m_output_data_path(output_data_path)
    , m_input_data_path(input_data_path)
    , m_grid(grid)
    , m_tiler(tiler)
    , m_tile_writer(std::move(tile_writer))
{
}

ParallelTileGenerator ParallelTileGenerator::make(const std::string& input_data_path,
    ctb::Grid::Srs srs, radix::tile::Scheme tiling_scheme,
    std::unique_ptr<ParallelTileWriterInterface> tile_writer,
    const std::string& output_data_path,
    unsigned grid_resolution)
{
    const auto dataset = Dataset::make_shared(input_data_path);
    ctb::Grid grid = ctb::GlobalGeodetic(grid_resolution);
    if (srs == ctb::Grid::Srs::SphericalMercator)
        grid = ctb::GlobalMercator(grid_resolution);
    const auto border = tile_writer->formatRequiresBorder();
    return { input_data_path, grid, ParallelTiler(grid, dataset->bounds(grid.getSRS()), border, tiling_scheme), std::move(tile_writer), output_data_path };
}

const ParallelTiler& ParallelTileGenerator::tiler() const
{
    return m_tiler;
}

const ctb::Grid& ParallelTileGenerator::grid() const
{
    return m_grid;
}

void ParallelTileGenerator::write(const radix::tile::Descriptor& tile, const HeightData& heights) const
{
    const auto dir_path = fmt::format("{}/{}/{}", m_output_data_path, tile.id.zoom_level, tile.id.coords.x);
    const auto file_path = fmt::format("{}/{}.{}", dir_path, tile.id.coords.y, m_tile_writer->formatFileEnding());
    std::filesystem::create_directories(dir_path);
    m_tile_writer->write(file_path, tile, heights);
}

void ParallelTileGenerator::process(const std::pair<ctb::i_zoom, ctb::i_zoom>& zoom_range, bool progress_bar_on_console, bool generate_world_wide_tiles) const
{
    auto tiler = m_tiler;
    if (generate_world_wide_tiles)
        tiler.setBounds(grid().getExtent());

    const auto tiles = tiler.generateTiles(zoom_range);

    auto pi = ProgressIndicator(tiles.size());
    std::jthread monitoring_thread;

    if (progress_bar_on_console)
        monitoring_thread = pi.start_monitoring(); // destructor will join.

    const auto fun = [&pi, progress_bar_on_console, this](const radix::tile::Descriptor& tile) {
        // Recreating Dataset for every tile. This was the easiest fix for multithreading,
        // and it takes only 0.5% of the time (half a percent).
        // most of the cpu time is used in 'readWithOverviews' (specificly 'RasterIO', and
        // 'VRTWarpedRasterBand::IReadBlock') and a bit in 'write' (specifically 'FreeImage_Save').
        const auto dataset = Dataset::make_shared(m_input_data_path);
        DatasetReader reader(dataset, m_grid.getSRS(), 1, m_warn_on_missing_overviews);
        const auto heights = reader.readWithOverviews(tile.srsBounds, tile.tileSize, tile.tileSize);
        write(tile, heights);
        if (progress_bar_on_console)
            pi.task_finished();
    };
    std::for_each(std::execution::par, tiles.begin(), tiles.end(), fun);
}

radix::tile::Border ParallelTileWriterInterface::formatRequiresBorder() const
{
    return m_format_requires_border;
}

const std::string& ParallelTileWriterInterface::formatFileEnding() const
{
    return m_file_ending;
}
