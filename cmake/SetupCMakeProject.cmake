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

    string(JOIN " " _alp_sanitizer_flags ${ALP_SANITIZER_FLAGS})

    execute_process(
        COMMAND ${CMAKE_COMMAND}
                -G ${CMAKE_GENERATOR}
                -S ${SRC_DIR}
                -B ${BUILD_DIR}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS} ${_alp_sanitizer_flags}"
                "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS} ${_alp_sanitizer_flags}"
                "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS} ${_alp_sanitizer_flags}"
                "-DCMAKE_MODULE_LINKER_FLAGS=${CMAKE_MODULE_LINKER_FLAGS} ${_alp_sanitizer_flags}"
                "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS} ${_alp_sanitizer_flags}"
                "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
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
                --parallel
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
    set(build_dir   "${CMAKE_BINARY_DIR}/alp_external/${arg_NAME}_build")
    set(install_dir "${CMAKE_BINARY_DIR}/alp_external/${arg_NAME}")
    set(stamp_file  "${install_dir}/.alp_install_signature")
    set(version_signature "${arg_COMMITISH}${arg_CMAKE_ARGUMENTS}${ALP_SANITIZER_FLAGS}")
    set(cache_signature "URL=${arg_URL}
COMMITISH=${arg_COMMITISH}
BUILD_TYPE=${CMAKE_BUILD_TYPE}
SYSTEM=${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}
CXX=${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}
SANITIZER_FLAGS=${ALP_SANITIZER_FLAGS}
ARGS=${arg_CMAKE_ARGUMENTS}")

    if(DEFINED ${version_var} AND "${${version_var}}" STREQUAL "${version_signature}" AND DEFINED ${path_var} AND EXISTS "${${path_var}}")
        list(PREPEND CMAKE_PREFIX_PATH "${${path_var}}")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${install_dir}" AND EXISTS "${stamp_file}")
        file(READ "${stamp_file}" installed_signature)
        if(installed_signature STREQUAL cache_signature)
            message(STATUS "[alp] Using cached install for ${arg_NAME}: ${install_dir}")
            list(PREPEND CMAKE_PREFIX_PATH "${install_dir}")
            set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
            set(${path_var}    "${install_dir}"   CACHE PATH   "Install path for ${arg_NAME}"            FORCE)
            set(${version_var} "${version_signature}" CACHE STRING "Installed commit/tag for $ + build flags"    FORCE)
            return()
        endif()
    endif()

    alp_add_git_repository(${arg_NAME} URL ${arg_URL} COMMITISH ${arg_COMMITISH} DO_NOT_ADD_SUBPROJECT)
    set(src_dir     "${${arg_NAME}_SOURCE_DIR}")

    file(REMOVE_RECURSE "${install_dir}")
    _alp_build_and_install(${arg_NAME} ${src_dir} ${build_dir} ${install_dir} ${arg_CMAKE_ARGUMENTS})
    file(WRITE "${stamp_file}" "${cache_signature}")

    list(PREPEND CMAKE_PREFIX_PATH "${install_dir}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)


    set(${path_var}    "${install_dir}"   CACHE PATH   "Install path for ${arg_NAME}"            FORCE)
    set(${version_var} "${version_signature}" CACHE STRING "Installed commit/tag for $ + build flags"    FORCE)
endfunction()
