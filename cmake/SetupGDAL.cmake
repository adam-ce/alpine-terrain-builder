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

if(NOT COMMAND alp_add_git_repository)
    include(${CMAKE_CURRENT_LIST_DIR}/AddRepo.cmake)
endif()

function(_alp_build_and_install NAME SRC_DIR BUILD_DIR INSTALL_DIR)
    if(NOT EXISTS "${INSTALL_DIR}/lib" AND
       NOT EXISTS "${INSTALL_DIR}/lib64")
        message(STATUS "[alp] Configuring ${NAME}")
        # file(MAKE_DIRECTORY "${BUILD_DIR}")

        execute_process(
            COMMAND ${CMAKE_COMMAND}
                    -G ${CMAKE_GENERATOR}
                    -S ${SRC_DIR}
                    -B ${BUILD_DIR}
                    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR}
                    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                    ${ARGN}
            RESULT_VARIABLE _cfg_res)

        if(_cfg_res)
            message(FATAL_ERROR "[alp] Configuring ${NAME} failed!")
        endif()

        message(STATUS "[alp] Building + installing ${NAME}")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                    --build ${BUILD_DIR}
                    --config ${CMAKE_BUILD_TYPE}
                    --target install
            RESULT_VARIABLE _bld_res)

        if(_bld_res)
            message(FATAL_ERROR "[alp] Building ${NAME} failed!")
        endif()
    else()
        message(STATUS "[alp] Re‑using existing ${NAME} install at ${INSTALL_DIR}")
    endif()
endfunction()

function(alp_setup_proj proj_version)
    alp_add_git_repository(proj URL https://github.com/OSGeo/PROJ.git COMMITISH ${proj_version} DO_NOT_ADD_SUBPROJECT)

    set(_proj_src      "${proj_SOURCE_DIR}")
    set(_proj_build    "${CMAKE_BINARY_DIR}/alp_external/proj_build")
    set(_proj_install  "${CMAKE_BINARY_DIR}/alp_external/proj_install")

    _alp_build_and_install(PROJ ${_proj_src} ${_proj_build} ${_proj_install} -DBUILD_TESTING=OFF -DBUILD_APPS=OFF)

    set(ALP_PROJ_INSTALL_DIR ${_proj_install} CACHE PATH "Local PROJ install" FORCE)

    list(PREPEND CMAKE_PREFIX_PATH "${_proj_install}")
    find_package(PROJ CONFIG REQUIRED)
endfunction()

function(alp_setup_gdal)
    set(oneValueArgs GDAL_VERSION PROJ_VERSION)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})

    if(NOT ARG_GDAL_VERSION OR NOT ARG_PROJ_VERSION)
        message(FATAL_ERROR "alp_setup_gdal() needs: GDAL_VERSION <tag> PROJ_VERSION <tag>")
    endif()

    alp_setup_proj(${ARG_PROJ_VERSION})
    set(_proj_install "${ALP_PROJ_INSTALL_DIR}")

    alp_add_git_repository(gdal URL https://github.com/OSGeo/gdal.git COMMITISH ${ARG_GDAL_VERSION} DO_NOT_ADD_SUBPROJECT)

    set(_gdal_src      "${gdal_SOURCE_DIR}")
    set(_gdal_build    "${CMAKE_BINARY_DIR}/alp_external/gdal_build")
    set(_gdal_install  "${CMAKE_BINARY_DIR}/alp_external/gdal_install")

    _alp_build_and_install(GDAL
        "${_gdal_src}" "${_gdal_build}" "${_gdal_install}"
        -DCMAKE_PREFIX_PATH="${_proj_install}"
        -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF
        -DOGR_BUILD_OPTIONAL_DRIVERS=OFF
        -DBUILD_APPS=OFF
        -DBUILD_TESTING=OFF
        -DBUILD_PYTHON_BINDINGS=OFF
        -DBUILD_JAVA_BINDINGS=OFF
        -DBUILD_CSHARP_BINDINGS=OFF
        -DGDAL_USE_JPEG=OFF
        -DGDAL_USE_ICONV=OFF)

    list(PREPEND CMAKE_PREFIX_PATH "${_gdal_install}")
    find_package(GDAL CONFIG REQUIRED)
endfunction()

