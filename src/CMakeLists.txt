cmake_minimum_required(VERSION 3.19)
project(terrain_builder_src LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

option(ALP_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_OVERVIEW_READING "enable GDAL overview reading (broken with newer GDAL versions)" OFF)

find_package(TBB REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

alp_add_git_repository(fmt URL https://github.com/fmtlib/fmt COMMITISH 10.2.1)
alp_add_git_repository(cli11 URL https://github.com/CLIUtils/CLI11.git COMMITISH 20de8b73bbbabaf2f94dd07c4ece8ff3590af531)

alp_add_git_repository(eigen URL https://gitlab.com/libeigen/eigen.git COMMITISH 3.4.0 DO_NOT_ADD_SUBPROJECT)
add_library(Eigen3 INTERFACE)
target_include_directories(Eigen3 INTERFACE ${eigen_SOURCE_DIR})

alp_setup_cmake_project(boost URL https://github.com/boostorg/boost.git COMMITISH "boost-1.88.0" CMAKE_ARGUMENTS -DBOOST_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF "-DBOOST_INCLUDE_LIBRARIES='graph'")
find_package(Boost CONFIG REQUIRED)

include(../cmake/SetupGDAL.cmake)
alp_setup_gdal(GDAL_VERSION v3.10.3 PROJ_VERSION 9.6.0)

alp_setup_cmake_project(cgal URL https://github.com/CGAL/cgal.git COMMITISH "v6.0.1" CMAKE_ARGUMENTS -DCGAL_HEADER_ONLY=ON -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF)
find_package(CGAL REQUIRED)

include(../cmake/SetupOpenCV.cmake)
alp_setup_opencv(4.11.0)

alp_add_git_repository(cgltf URL https://github.com/jkuhlmann/cgltf COMMITISH 7331a5adf4b0fde15f0e682a5803e4f17137e0ad DO_NOT_ADD_SUBPROJECT)
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
alp_add_git_repository(tinygltf URL https://github.com/syoyo/tinygltf COMMITISH 4fea26f6c8652f545560807bccc934cf0cdd86dd)

alp_add_git_repository(stb URL https://github.com/nothings/stb COMMITISH f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe DO_NOT_ADD_SUBPROJECT)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

alp_add_git_repository(tl_expected URL https://github.com/TartanLlama/expected.git COMMITISH v1.1.0 DO_NOT_ADD_SUBPROJECT)
add_library(tl_expected INTERFACE)
target_include_directories(tl_expected INTERFACE ${tl_expected_SOURCE_DIR}/include)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "SPDLOG_FMT_EXTERNAL" FORCE)
alp_add_git_repository(spdlog URL https://github.com/gabime/spdlog.git COMMITISH v1.13.0)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

alp_add_git_repository(zpp_bits URL https://github.com/eyalz800/zpp_bits.git COMMITISH v4.5 DO_NOT_ADD_SUBPROJECT)
add_library(zpp_bits INTERFACE)
target_include_directories(zpp_bits INTERFACE ${zpp_bits_SOURCE_DIR})

alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH af07738928ab3129899dde72c8e73ed01622a420 NOT_SYSTEM DEEP_CLONE)

add_library(terrainlib
    terrainlib/ctb/CTBException.hpp
    terrainlib/ctb/GlobalGeodetic.hpp terrainlib/ctb/GlobalGeodetic.cpp
    terrainlib/ctb/GlobalMercator.hpp terrainlib/ctb/GlobalMercator.cpp
    terrainlib/ctb/Grid.hpp
    terrainlib/ctb/TileCoordinate.hpp
    terrainlib/ctb/types.hpp

    terrainlib/mesh/io/error.h
    terrainlib/mesh/io/gltf.h terrainlib/mesh/io/gltf.cpp
    terrainlib/mesh/io/options.h
    terrainlib/mesh/io/terrain.h terrainlib/mesh/io/terrain.cpp
    terrainlib/mesh/io/texture.h terrainlib/mesh/io/texture.cpp
    terrainlib/mesh/io/utils.h terrainlib/mesh/io/utils.cpp

    terrainlib/mesh/io.h terrainlib/mesh/io.cpp
    terrainlib/mesh/SimpleMesh.h
    terrainlib/mesh/utils.h terrainlib/mesh/utils.cpp

    terrainlib/octree/id.h
    terrainlib/octree/storage.h
    terrainlib/octree/space.h
    terrainlib/octree/utils.h

    terrainlib/Dataset.h terrainlib/Dataset.cpp
    terrainlib/init.h terrainlib/init.cpp
    terrainlib/log.h terrainlib/log.cpp
    terrainlib/ProgressIndicator.h terrainlib/ProgressIndicator.cpp
    terrainlib/srs.h
)
target_include_directories(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainlib)
target_link_libraries(terrainlib PUBLIC radix ZLIB::ZLIB GDAL::GDAL spdlog fmt zpp_bits cgltf TBB::tbb tl_expected opencv_core opencv_imgproc opencv_imgcodecs)

if(ALP_ENABLE_OVERVIEW_READING)
    target_compile_definitions(terrainlib PUBLIC DATB_ENABLE_OVERVIEW_READING)
endif()

add_library(terrainbuilderlib
    terrainbuilder/tile_provider.h
    terrainbuilder/mesh_builder.h
    terrainbuilder/mesh_builder.cpp
    terrainbuilder/raw_dataset_reader.h
    terrainbuilder/texture_assembler.h
    terrainbuilder/border.h
    terrainbuilder/raster.h
    terrainbuilder/terrainbuilder.h
    terrainbuilder/terrainbuilder.cpp
)
target_include_directories(terrainbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainbuilder)
target_link_libraries(terrainbuilderlib PUBLIC terrainlib spdlog CLI11::CLI11 tl_expected)
add_executable(terrainbuilder
    terrainbuilder/main.cpp
)
target_link_libraries(terrainbuilder PUBLIC terrainbuilderlib)

add_library(terrainmergerlib
    terrainmerger/cli.h
    terrainmerger/cli.cpp
    terrainmerger/merge.h
    terrainmerger/merge.cpp
    terrainmerger/simplify.h
    terrainmerger/simplify.cpp
    terrainmerger/uv_map.h
    terrainmerger/uv_map.cpp
    terrainmerger/cgal.h
    terrainmerger/convert.h
    terrainmerger/convert.cpp
    terrainmerger/validate.h
)
target_include_directories(terrainmergerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainmerger)
target_precompile_headers(terrainmergerlib PRIVATE terrainmerger/pch.h)
target_link_libraries(terrainmergerlib PUBLIC terrainlib radix CLI11::CLI11 Eigen3 CGAL::CGAL)
add_executable(terrainmerger
    terrainmerger/main.cpp
)
target_link_libraries(terrainmerger PUBLIC terrainmergerlib)

add_executable(tile-downloader
    tile_downloader/main.cpp
)
target_link_libraries(tile-downloader PUBLIC terrainlib ${CURL_LIBRARIES})

add_executable(terrainconvert
    terrainconvert/cli.h
    terrainconvert/cli.cpp
    terrainconvert/main.cpp
)
target_link_libraries(terrainconvert PUBLIC terrainlib CLI11::CLI11)
