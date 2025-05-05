cmake_minimum_required(VERSION 3.19)
project(terrain_builder_src LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

option(ALP_ENABLE_THREAD_SANITIZER "compiles atb with thread sanitizer enabled (only debug, works only on g++ and clang)" OFF)
option(ALP_ENABLE_OVERVIEW_READING "enable GDAL overview reading (broken with newer GDAL versions)" OFF)



# alp_add_git_repository(PROJ URL https://github.com/OSGeo/PROJ.git COMMITISH 9.6.0)
# # set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/alp_external/PROJ" ${CMAKE_PREFIX_PATH})

# set(PROJ_ROOT ${CMAKE_BINARY_DIR}/alp_external/PROJ CACHE INTERNAL "unsure this is correct")
# set(PROJ_DIR ${CMAKE_BINARY_DIR}/alp_external/PROJ CACHE INTERNAL "unsure this is correct")
# set(PROJ_INCLUDE_DIR ${CMAKE_BINARY_DIR}/alp_external/PROJ/include CACHE INTERNAL "this should be correct")
# set(PROJ_LIBRARY PROJ::proj CACHE INTERNAL "hope this works")

# can't include proj via repo, gdals find script doesn't play well with it.
# todo: clone cgal, clean up

set(GDAL_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "" FORCE)
set(OGR_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "" FORCE)
set(BUILD_APPS OFF CACHE BOOL "don't build gdal apps" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "don't build gdal apps" FORCE)
set(BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(BUILD_JAVA_BINDINGS OFF CACHE BOOL "" FORCE)
set(BUILD_CSHARP_BINDINGS OFF CACHE BOOL "" FORCE)
set(GDAL_USE_JPEG OFF CACHE BOOL "not targetting jpeg for now and it builds an additonal executable" FORCE)

# The Iconv library is used to convert text from one encoding to another encoding. It is generally available as a system library for Unix-like systems.
# On Windows, GDAL can leverage the API of the operating system for a few base conversions, but using Iconv will provide additional capabilities.
# gdal supports vector layers, we won't need it here for now, and an unnecessary target will be produced.
set(GDAL_USE_ICONV OFF CACHE BOOL "i don't think we need this one" FORCE)
alp_add_git_repository(gdal URL https://github.com/OSGeo/gdal.git COMMITISH v3.10.3)

find_package(TBB REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CURL REQUIRED)

alp_add_git_repository(fmt URL https://github.com/fmtlib/fmt COMMITISH 10.2.1)
alp_add_git_repository(cli11 URL https://github.com/CLIUtils/CLI11.git COMMITISH 20de8b73bbbabaf2f94dd07c4ece8ff3590af531)

alp_add_git_repository(eigen URL https://gitlab.com/libeigen/eigen.git COMMITISH 3.4.0 DO_NOT_ADD_SUBPROJECT)
add_library(Eigen3 INTERFACE)
target_include_directories(Eigen3 INTERFACE ${eigen_SOURCE_DIR})

include(../cmake/SetupCGAL.cmake)
alp_setup_cgal("v6.0.1")

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

alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH 5d56bcefbb8f13ebedebdf30bc24c92708566a70 NOT_SYSTEM)

add_library(terrainlib
    terrainlib/alpine_raster.h terrainlib/alpine_raster.cpp

    terrainlib/mesh/io.h terrainlib/mesh/io.cpp
    terrainlib/mesh/raw_gltf.h terrainlib/mesh/raw_gltf.cpp
    terrainlib/mesh/terrain_mesh.h terrainlib/mesh/terrain_mesh.cpp
    terrainlib/mesh/non_copyable.h
    terrainlib/log.h
    terrainlib/log.cpp

    terrainlib/Dataset.h terrainlib/Dataset.cpp
    terrainlib/DatasetReader.h terrainlib/DatasetReader.cpp
    terrainlib/depth_first_tile_traverser.h
    terrainlib/Exception.h
    terrainlib/Image.h terrainlib/Image.cpp
    terrainlib/ParallelTileGenerator.h terrainlib/ParallelTileGenerator.cpp
    terrainlib/ParallelTiler.h terrainlib/ParallelTiler.cpp
    terrainlib/ProgressIndicator.h terrainlib/ProgressIndicator.cpp
    terrainlib/srs.h
    terrainlib/TileHeightsGenerator.h terrainlib/TileHeightsGenerator.cpp
    terrainlib/Tiler.h terrainlib/Tiler.cpp
    terrainlib/TopDownTiler.h terrainlib/TopDownTiler.cpp
    terrainlib/ctb/CTBException.hpp
    terrainlib/ctb/GlobalGeodetic.hpp terrainlib/ctb/GlobalGeodetic.cpp
    terrainlib/ctb/GlobalMercator.hpp terrainlib/ctb/GlobalMercator.cpp
    terrainlib/ctb/Grid.hpp
    terrainlib/ctb/TileCoordinate.hpp
    terrainlib/ctb/types.hpp
    terrainlib/init.h terrainlib/init.cpp
)
target_link_libraries(terrainlib PUBLIC radix ZLIB::ZLIB GDAL::GDAL spdlog fmt zpp_bits freeimage cgltf TBB::tbb tl_expected ${OpenCV_LIBS})
target_include_directories(terrainlib PUBLIC terrainlib ${OpenCV_INCLUDE_DIRS})
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
    terrainbuilder/octree/id.h
    terrainbuilder/octree/storage.h
    terrainbuilder/octree/space.h
)
target_include_directories(terrainbuilderlib PUBLIC terrainbuilder terrainlib)
target_link_libraries(terrainbuilderlib PUBLIC terrainlib spdlog CLI11::CLI11 tl_expected)
add_executable(terrainbuilder
    terrainbuilder/main.cpp
)
target_include_directories(terrainbuilder PUBLIC terrainbuilder)
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
target_precompile_headers(terrainmergerlib PRIVATE terrainmerger/pch.h)
target_include_directories(terrainmergerlib PUBLIC terrainmerger terrainlib)
target_link_libraries(terrainmergerlib PUBLIC terrainlib radix CLI11::CLI11 Eigen3 CGAL::CGAL)
add_executable(terrainmerger
    terrainmerger/main.cpp
)
target_include_directories(terrainmerger PUBLIC terrainmerger)
target_link_libraries(terrainmerger PUBLIC terrainmergerlib)

add_executable(tile-downloader
    tile_downloader/main.cpp
)
target_link_libraries(tile-downloader PUBLIC terrainlib ${CURL_LIBRARIES})
target_include_directories(tile-downloader PUBLIC ${CURL_INCLUDE_DIR})

add_executable(terrainconvert
    terrainconvert/cli.h
    terrainconvert/cli.cpp
    terrainconvert/main.cpp
)
target_include_directories(terrainconvert PUBLIC terrainlib)
target_link_libraries(terrainconvert PUBLIC terrainlib CLI11::CLI11)
