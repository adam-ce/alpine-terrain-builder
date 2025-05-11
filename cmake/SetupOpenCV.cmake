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

function(alp_setup_opencv version)
    alp_setup_cmake_project(opencv
        URL https://github.com/opencv/opencv.git COMMITISH ${version}
        CMAKE_ARGUMENTS
            -DBUILD_TESTS=OFF
            -DBUILD_PERF_TESTS=OFF
            -DBUILD_EXAMPLES=OFF
            -DBUILD_opencv_apps=OFF
            -DBUILD_opencv_apps=OFF
            -DBUILD_opencv_python3=OFF
            -DBUILD_opencv_java=OFF
            -DBUILD_opencv_world=OFF
            -DWITH_OPENGL=OFF
            -DWITH_GSTREAMER=OFF
            -DWITH_FFMPEG=OFF
            -DWITH_JPEG=ON -DWITH_PNG=ON -DWITH_TIFF=ON
            -DWITH_GTK=OFF -DWITH_QT=OFF
            -DBUILD_LIST=core,imgproc,imgcodecs
        )
    find_package(OpenCV REQUIRED)
endfunction()

