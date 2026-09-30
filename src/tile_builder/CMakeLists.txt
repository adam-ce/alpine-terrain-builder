# *****************************************************************************
# AlpineMaps.org
# Copyright (C) 2026 Adam Celarek-Litofcenko
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************

add_library(tilebuilderlib
    Exception.h
    ParallelTileGenerator.h ParallelTileGenerator.cpp
    ParallelTiler.h ParallelTiler.cpp
    TileHeightsGenerator.h TileHeightsGenerator.cpp
    Tiler.h Tiler.cpp
    TopDownTiler.h TopDownTiler.cpp
    alpine_raster.h alpine_raster.cpp
    depth_first_tile_traverser.h
    image_writer.h image_writer.cpp
)

target_include_directories(tilebuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tilebuilderlib PUBLIC terrainlib ZLIB::ZLIB GDAL::GDAL fmt)

add_executable(tile-builder main.cpp)
target_link_libraries(tile-builder PRIVATE tilebuilderlib)
