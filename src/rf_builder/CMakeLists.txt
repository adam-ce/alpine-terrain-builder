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

add_library(rfbuilderlib
    Mask.h Mask.cpp
    gdal/TileWorker.h
    run.h run.cpp
    tiles/provider.h tiles/provider.cpp
    tiles/HttpClient.h tiles/HttpClient.cpp
    tiles/planning.h tiles/planning.cpp
    tiles/inputs.h tiles/inputs.cpp
    tiles/TileWorker.h tiles/TileWorker.cpp
    tiles/build.h tiles/build.cpp
    tiles/cli.h tiles/cli.cpp
    gdal/cli.h gdal/cli.cpp
    gdal/build.h gdal/build.cpp
    gdal/inputs.h gdal/inputs.cpp
    gdal/planning.h gdal/planning.cpp
    gdal/nodata.h gdal/nodata.cpp
)
target_include_directories(rfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rfbuilderlib PUBLIC terrainlib CLI11::CLI11 CURL::libcurl)

add_executable(rf-builder main.cpp)
target_link_libraries(rf-builder PRIVATE rfbuilderlib CLI11::CLI11)
