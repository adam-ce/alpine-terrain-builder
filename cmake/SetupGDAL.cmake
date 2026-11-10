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

option(ALP_GDAL_ENABLE_ECW "Build GDAL with ECW support (requires a separately licensed ECW SDK)" OFF)

function(alp_setup_gdal)
    set(oneValueArgs GDAL_VERSION GDAL_SHA256 GEOS_VERSION GEOS_SHA256 PROJ_VERSION PROJ_SHA256)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_GDAL_VERSION OR NOT ARG_GDAL_SHA256
            OR NOT ARG_GEOS_VERSION OR NOT ARG_GEOS_SHA256
            OR NOT ARG_PROJ_VERSION OR NOT ARG_PROJ_SHA256)
        message(FATAL_ERROR "alp_setup_gdal() needs VERSION and SHA256 arguments for GDAL, GEOS and PROJ")
    endif()

    alp_setup_cmake_project(proj
        URL https://download.osgeo.org/proj/proj-${ARG_PROJ_VERSION}.tar.gz
        SHA256 ${ARG_PROJ_SHA256}
        CMAKE_ARGUMENTS -DBUILD_TESTING=OFF -DBUILD_APPS=OFF)
    find_package(PROJ CONFIG REQUIRED)

    alp_setup_cmake_project(geos
        URL https://download.osgeo.org/geos/geos-${ARG_GEOS_VERSION}.tar.bz2
        SHA256 ${ARG_GEOS_SHA256}
        CMAKE_ARGUMENTS
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTING=OFF
            -DGEOS_BUILD_DEVELOPER=OFF
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    )

    set(_ecw_args
        "-DGDAL_USE_ECW:BOOL=${ALP_GDAL_ENABLE_ECW}"
        "-DGDAL_ENABLE_DRIVER_ECW:BOOL=${ALP_GDAL_ENABLE_ECW}")
    if(ALP_GDAL_ENABLE_ECW)
        # Let GDAL's FindECW select the SDK ABI and reject missing/unsupported SDKs.
        # Forward the environment hint explicitly so it also enters the build cache key.
        if(NOT DEFINED ECW_ROOT AND DEFINED ENV{ECW_ROOT})
            set(ECW_ROOT "$ENV{ECW_ROOT}")
        endif()
        _alp_append_cache_arg(_ecw_args ECW_ROOT PATH)
        _alp_append_cache_arg(_ecw_args ECW_INCLUDE_DIR PATH)
        _alp_append_cache_arg(_ecw_args ECW_LIBRARY FILEPATH)
        list(APPEND _ecw_args
            -DGDAL_ENABLE_DRIVER_ECW_PLUGIN:BOOL=OFF
            -DCMAKE_INSTALL_RPATH_USE_LINK_PATH:BOOL=ON)
    endif()

    string(REGEX REPLACE "^v" "" _gdal_version "${ARG_GDAL_VERSION}")
    alp_setup_cmake_project(gdal
        URL https://github.com/OSGeo/gdal/releases/download/v${_gdal_version}/gdal-${_gdal_version}.tar.gz
        SHA256 ${ARG_GDAL_SHA256}
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
            -DGDAL_USE_CURL=ON
            -DGDAL_USE_GEOS=ON
            -DGDAL_USE_SQLITE3=ON
            "-DCMAKE_INSTALL_RPATH=\$ORIGIN/../../proj/lib"
            ${_ecw_args}
        )

    find_package(GDAL CONFIG REQUIRED)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()
