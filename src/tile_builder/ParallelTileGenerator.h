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

#ifndef PARALLELTILEGENERATOR_H
#define PARALLELTILEGENERATOR_H

#include <memory>
#include <string>

#include "Image.h"
#include "ParallelTiler.h"
#include "ctb/Grid.hpp"
#include "ctb/types.hpp"
#include <radix/tile.h>

class ParallelTileWriterInterface;

class ParallelTileGenerator {
    std::string m_output_data_path;
    std::string m_input_data_path;
    ctb::Grid m_grid;
    ParallelTiler m_tiler;
    std::unique_ptr<ParallelTileWriterInterface> m_tile_writer;
    bool m_warn_on_missing_overviews = true;

public:
    ParallelTileGenerator(const std::string& input_data_path, const ctb::Grid& grid, const ParallelTiler& tiler, std::unique_ptr<ParallelTileWriterInterface> tile_writer, const std::string& output_data_path);
    [[nodiscard]] static ParallelTileGenerator make(const std::string& input_data_path,
        ctb::Grid::Srs srs, radix::tile::Scheme tiling_scheme,
        std::unique_ptr<ParallelTileWriterInterface> tile_writer,
        const std::string& output_data_path,
        unsigned grid_resolution = 256);

    void setWarnOnMissingOverviews(bool flag) { m_warn_on_missing_overviews = flag; }
    [[nodiscard]] const ParallelTiler& tiler() const;
    [[nodiscard]] const ctb::Grid& grid() const;
    void write(const radix::tile::Descriptor& tile, const HeightData& heights) const;
    void process(const std::pair<ctb::i_zoom, ctb::i_zoom>& zoom_range, bool progress_bar_on_console = false, bool generate_world_wide_tiles = false) const;
};

class ParallelTileWriterInterface {
    radix::tile::Border m_format_requires_border;
    std::string m_file_ending;

public:
    ParallelTileWriterInterface(radix::tile::Border format_requires_border, const std::string& file_ending)
        : m_format_requires_border(format_requires_border)
        , m_file_ending(file_ending)
    {
    }
    ParallelTileWriterInterface(const ParallelTileWriterInterface&) = default;
    ParallelTileWriterInterface(ParallelTileWriterInterface&&) = default;
    virtual ~ParallelTileWriterInterface() = default;
    ParallelTileWriterInterface& operator=(const ParallelTileWriterInterface&) = default;
    ParallelTileWriterInterface& operator=(ParallelTileWriterInterface&&) = default;
    virtual void write(const std::string& file_path, const radix::tile::Descriptor& tile, const HeightData& heights) const = 0;
    [[nodiscard]] radix::tile::Border formatRequiresBorder() const;
    [[nodiscard]] const std::string& formatFileEnding() const;
};

#endif // PARALLELTILEGENERATOR_H
