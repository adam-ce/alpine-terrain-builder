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

add_library(rfmergerlib
    cli.h cli.cpp
    inputs.h inputs.cpp
    merge.h merge.cpp
    partition.h partition.cpp
    priorities.h priorities.cpp
    produce.h produce.cpp
    selection.h
    statistics.h statistics.cpp
)
target_include_directories(rfmergerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rfmergerlib PUBLIC terrainlib CLI11::CLI11)

add_executable(rf-merger main.cpp)
target_link_libraries(rf-merger PRIVATE rfmergerlib CLI11::CLI11)
