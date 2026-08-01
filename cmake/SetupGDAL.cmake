#############################################################################
# AlpineMaps.org
# Copyright (C) 2025 Adam Celarek <family name at cg tuwien ac at>
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
#############################################################################

if(NOT COMMAND alp_setup_cmake_project)
    include(${CMAKE_CURRENT_LIST_DIR}/SetupCMakeProject.cmake)
endif()

function(alp_setup_gdal)
    set(oneValueArgs GDAL_VERSION GEOS_VERSION PROJ_VERSION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_GDAL_VERSION OR NOT ARG_GEOS_VERSION OR NOT ARG_PROJ_VERSION)
        message(FATAL_ERROR "alp_setup_gdal() needs: GDAL_VERSION <tag> GEOS_VERSION <tag> PROJ_VERSION <tag>")
    endif()

    alp_setup_cmake_project(proj URL https://github.com/OSGeo/PROJ.git COMMITISH ${ARG_PROJ_VERSION} CMAKE_ARGUMENTS -DBUILD_TESTING=OFF -DBUILD_APPS=OFF)
    find_package(PROJ CONFIG REQUIRED)

    alp_setup_cmake_project(geos
        URL https://github.com/libgeos/geos.git
        COMMITISH ${ARG_GEOS_VERSION}
        CMAKE_ARGUMENTS
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTING=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        )

    alp_setup_cmake_project(gdal
        URL https://github.com/OSGeo/gdal.git
        COMMITISH ${ARG_GDAL_VERSION}
        CMAKE_ARGUMENTS
            -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF
            -DGDAL_ENABLE_DRIVER_HFA=ON
            -DOGR_BUILD_OPTIONAL_DRIVERS=OFF
            -DOGR_ENABLE_DRIVER_GPKG=ON
            -DOGR_ENABLE_DRIVER_SQLITE=ON
            -DBUILD_APPS=OFF
            -DBUILD_TESTING=OFF
            -DBUILD_PYTHON_BINDINGS=OFF
            -DBUILD_JAVA_BINDINGS=OFF
            -DBUILD_CSHARP_BINDINGS=OFF
            -DGDAL_USE_ICONV=OFF
            -DGDAL_USE_EXTERNAL_LIBS=OFF
            -DGDAL_USE_GEOS=ON
            -DGDAL_USE_SQLITE3=ON
            "-DCMAKE_INSTALL_RPATH=\$ORIGIN/../../proj/lib"
        )

    find_package(GDAL CONFIG REQUIRED)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()
