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


include(ExternalProject)

function(alp_setup_cgal cgal_version)
    alp_add_git_repository(cgal URL https://github.com/CGAL/cgal.git COMMITISH ${cgal_version} DO_NOT_ADD_SUBPROJECT)

    set(cgal_destination_path "${CMAKE_BINARY_DIR}/alp_external/cgal")

    # If CGAL is already there, just import it and return
    if(EXISTS "${cgal_destination_path}/lib/cmake/CGAL/CGALConfig.cmake")
        list(PREPEND CMAKE_PREFIX_PATH
             "${cgal_destination_path}/lib/cmake/CGAL")
        find_package(CGAL CONFIG REQUIRED)          # gives CGAL::CGAL etc.
        return()
    endif()

    # First run?  Build + install CGAL once
    ExternalProject_Add(CGAL
        PREFIX "${cgal_destination_path}"
        SOURCE_DIR   "${cgal_SOURCE_DIR}"

        CMAKE_ARGS
          -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
          -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
          -DCGAL_HEADER_ONLY=ON
          -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF

        BUILD_COMMAND   "${CMAKE_COMMAND}" --build <BINARY_DIR> --target install
        UPDATE_COMMAND  ""                          # updates handled by git
        STEP_TARGETS    install                     # creates CGAL-install
    )

    # Provide an imported interface target right
    file(MAKE_DIRECTORY "${cgal_destination_path}/include")
    add_library(CGAL::CGAL INTERFACE IMPORTED GLOBAL)
    set_target_properties(CGAL::CGAL PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${cgal_destination_path}/include")

    # make sure headers are installed *before* anything that includes them
    add_dependencies(CGAL::CGAL CGAL-install)
endfunction()
