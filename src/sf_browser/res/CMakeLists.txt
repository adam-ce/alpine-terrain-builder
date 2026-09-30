# *****************************************************************************
# AlpineMaps.org
# Copyright (C) 2025 Adrian Gawor
# Copyright (C) 2025 Adam Celarek-Litofcenko
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

# Default Resource Library
# Resources are accessed in the code by
# 1. Including the CMRC header file:                                    #include <cmrc/cmrc.hpp>
# 2. Placing the following macro at the global scope of the code file:  CMRC_DECLARE(<lib_namespace>);
# 3. Getting a handle for the resource library:                         auto r = cmrc::<lib_namespace>::get_filesystem();
# 4. Accessing a resource:                                              auto data = r.open(<resource_path>);
#
# For usage see: https://github.com/vector-of-bool/cmrc/tree/master#usage

cmrc_add_resource_library(
    sf-browser-resources
    NAMESPACE res

# ADD NEW RESOURCES AFTER HERE
    shaders/octree_lines.vert
    shaders/octree_lines.frag
    # shaders/select.csh
)

# Generated CMRC sources reuse internal symbol names and cannot share a unity translation unit.
set_target_properties(sf-browser-resources PROPERTIES UNITY_BUILD OFF)
