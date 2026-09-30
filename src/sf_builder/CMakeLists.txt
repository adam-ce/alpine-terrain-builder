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

add_library(sfbuilderlib
    border.h
    mesh_builder.h mesh_builder.cpp
    raw_dataset_reader.h
    terrainbuilder.h terrainbuilder.cpp
    texture_assembler.h
    tile_provider.h
)

target_include_directories(sfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfbuilderlib PUBLIC terrainlib spdlog)

add_executable(sf-builder main.cpp)
target_link_libraries(sf-builder PRIVATE sfbuilderlib CLI11::CLI11)
