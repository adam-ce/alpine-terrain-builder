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
    message(STATUS "[alp] Configuring ${NAME}")

    execute_process(
        COMMAND ${CMAKE_COMMAND}
                -G ${CMAKE_GENERATOR}
                -S ${SRC_DIR}
                -B ${BUILD_DIR}
                -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
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
endfunction()

function(alp_setup_cmake_project arg_NAME)
    set(options)
    set(oneValueArgs URL COMMITISH)
    set(multiValueArgs CMAKE_ARGUMENTS)
    cmake_parse_arguments(PARSE_ARGV 1 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

    if(NOT arg_NAME OR NOT arg_URL OR NOT arg_COMMITISH)
        message(FATAL_ERROR "[alp] alp_setup_cmake_project() needs: <name> URL <url> COMMITISH <tag>")
    endif()

    set(version_var "ALP_INSTALLED_${arg_NAME}_VERSION")
    set(path_var    "ALP_INSTALLED_${arg_NAME}_PATH")

    if(DEFINED ${version_var} AND "${${version_var}}" STREQUAL "${arg_COMMITISH}${arg_CMAKE_ARGUMENTS}" AND DEFINED ${path_var} AND EXISTS "${${path_var}}")
        list(PREPEND CMAKE_PREFIX_PATH "${${path_var}}")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
        return()
    endif()

    alp_add_git_repository(${arg_NAME} URL ${arg_URL} COMMITISH ${arg_COMMITISH} DO_NOT_ADD_SUBPROJECT)
    set(src_dir     "${${arg_NAME}_SOURCE_DIR}")
    set(build_dir   "${CMAKE_BINARY_DIR}/alp_external/${arg_NAME}_build")
    set(install_dir "${CMAKE_BINARY_DIR}/alp_external/${arg_NAME}")

    file(REMOVE_RECURSE "${install_dir}")
    _alp_build_and_install(${arg_NAME} ${src_dir} ${build_dir} ${install_dir} ${arg_CMAKE_ARGUMENTS})

    list(PREPEND CMAKE_PREFIX_PATH "${install_dir}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)


    set(${path_var}    "${install_dir}"   CACHE PATH   "Install path for ${arg_NAME}"            FORCE)
    set(${version_var} "${arg_COMMITISH}${arg_CMAKE_ARGUMENTS}" CACHE STRING "Installed commit/tag for $ + build flags"    FORCE)
endfunction()


