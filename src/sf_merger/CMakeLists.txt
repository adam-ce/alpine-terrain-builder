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

add_library(sfmergerlib INTERFACE
    NodeLoader.h
    NodeWriter.h
    cut.h
    earth.h
    mask.h
    merge.h
    utils.h

    atlas/HorizontalStripLayoutPlanner.h
    atlas/Layout.h
    atlas/LayoutPlanner.h
    atlas/Sheet.h
    atlas/build.h

    merge/NodeData.h
    merge/Result.h
    merge/visitor/Masked.h
    merge/visitor/Simple.h
    merge/visitor/Visitor.h
)

target_include_directories(sfmergerlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfmergerlib INTERFACE terrainlib)

add_executable(sf-merger
    cli.h cli.cpp
    main.cpp
)
target_link_libraries(sf-merger PRIVATE sfmergerlib CLI11::CLI11)
