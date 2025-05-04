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
# next: set gdal flags to build only lib, build only necessary formats, build alpine maps, clone cgal, replace FetchContent, clean up
set(GDAL_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "")
set(OGR_BUILD_OPTIONAL_DRIVERS OFF CACHE BOOL "")
set(BUILD_APPS OFF CACHE BOOL "")
alp_add_git_repository(gdal URL https://github.com/OSGeo/gdal.git COMMITISH v3.10.3)

find_package(TBB REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenCV REQUIRED)
find_package(CGAL REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)
find_package(CURL REQUIRED)


include(FetchContent)

if(NOT TARGET fmt::fmt)
    FetchContent_Declare(fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt
        GIT_TAG 10.2.1
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/fmt
    )
    FetchContent_MakeAvailable(fmt)
endif()

alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH bbed8f3607c2d5fa97e309b119d0520fbebca983 NOT_SYSTEM)

if(NOT TARGET cgltf)
    FetchContent_Declare(cgltf
        GIT_REPOSITORY https://github.com/jkuhlmann/cgltf
        GIT_TAG 7331a5adf4b0fde15f0e682a5803e4f17137e0ad
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/cgltf
    )
    FetchContent_MakeAvailable(cgltf)
    add_library(cgltf INTERFACE)
    target_include_directories(cgltf INTERFACE ${cgltf_SOURCE_DIR})
endif()

if(NOT TARGET tinygltf)
    set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
    set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
    FetchContent_Declare(tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf
        GIT_TAG 4fea26f6c8652f545560807bccc934cf0cdd86dd
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/tinygltf
    )
    FetchContent_MakeAvailable(tinygltf)
endif()

if(NOT TARGET stb)
    FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb
        GIT_TAG f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/stb
    )
    FetchContent_MakeAvailable(stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

if(NOT TARGET cli11)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/3rdparty/cli11)
        FetchContent_Declare(cli11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG 20de8b73bbbabaf2f94dd07c4ece8ff3590af531
            SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/cli11
        )
        FetchContent_MakeAvailable(cli11)
    else()
        add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/cli11 ${CMAKE_BINARY_DIR}/_deps/cli11-build)
    endif()
endif()

alp_add_git_repository(tl_expected URL https://github.com/TartanLlama/expected.git COMMITISH v1.1.0 DO_NOT_ADD_SUBPROJECT)
add_library(tl_expected INTERFACE)
target_include_directories(tl_expected INTERFACE ${tl_expected_SOURCE_DIR}/include)

if(NOT TARGET spdlog)
    set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "SPDLOG_FMT_EXTERNAL" FORCE)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/spdlog
    )
    FetchContent_MakeAvailable(spdlog)
    add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
endif()

if(NOT TARGET zpp_bits)
    FetchContent_Declare(zpp_bits
        GIT_REPOSITORY https://github.com/eyalz800/zpp_bits.git
        GIT_TAG v4.4.20
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/zpp_bits
    )
    FetchContent_MakeAvailable(zpp_bits)
    add_library(zpp_bits INTERFACE)
    target_include_directories(zpp_bits INTERFACE ${zpp_bits_SOURCE_DIR})
endif()

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
    terrainlib/algorithms/primitives.h
    terrainlib/algorithms/raster_triangle_scanline.h
    terrainlib/ctb/CTBException.hpp
    terrainlib/ctb/GlobalGeodetic.hpp terrainlib/ctb/GlobalGeodetic.cpp
    terrainlib/ctb/GlobalMercator.hpp terrainlib/ctb/GlobalMercator.cpp
    terrainlib/ctb/Grid.hpp
    terrainlib/ctb/TileCoordinate.hpp
    terrainlib/ctb/types.hpp
    terrainlib/tntn/BinaryIO.h terrainlib/tntn/BinaryIO.cpp
    terrainlib/tntn/Delaunator.h terrainlib/tntn/Delaunator.cpp
    terrainlib/tntn/DelaunayMesh.h terrainlib/tntn/DelaunayMesh.cpp
    terrainlib/tntn/DelaunayTriangle.h terrainlib/tntn/DelaunayTriangle.cpp
    terrainlib/tntn/endianness.h
    terrainlib/tntn/FileFormat.h
    terrainlib/tntn/File.h terrainlib/tntn/File.cpp
    terrainlib/tntn/gdal_init.h terrainlib/tntn/gdal_init.cpp
    terrainlib/tntn/geometrix.h terrainlib/tntn/geometrix.cpp
    terrainlib/tntn/logging.h terrainlib/tntn/logging.cpp
    terrainlib/tntn/Mesh2Raster.h terrainlib/tntn/Mesh2Raster.cpp
    terrainlib/tntn/Mesh.h terrainlib/tntn/Mesh.cpp
    terrainlib/tntn/MeshIO.h terrainlib/tntn/MeshIO.cpp
    terrainlib/tntn/MeshMode.h
    terrainlib/tntn/MeshWriter.h terrainlib/tntn/MeshWriter.cpp
    terrainlib/tntn/ObjPool.h
    terrainlib/tntn/OFFReader.h terrainlib/tntn/OFFReader.cpp
    terrainlib/tntn/Points2Mesh.h terrainlib/tntn/Points2Mesh.cpp
    terrainlib/tntn/QuadEdge.h terrainlib/tntn/QuadEdge.cpp
    terrainlib/tntn/QuantizedMeshIO.h terrainlib/tntn/QuantizedMeshIO.cpp
    terrainlib/tntn/Raster.h
    terrainlib/tntn/RasterIO.h terrainlib/tntn/RasterIO.cpp
    terrainlib/tntn/raster_tools.h terrainlib/tntn/raster_tools.cpp
    terrainlib/tntn/simple_meshing.h terrainlib/tntn/simple_meshing.cpp
    terrainlib/tntn/SuperTriangle.h terrainlib/tntn/SuperTriangle.cpp
    terrainlib/tntn/SurfacePoints.h terrainlib/tntn/SurfacePoints.cpp
    terrainlib/tntn/TerraMesh.h terrainlib/tntn/TerraMesh.cpp
    terrainlib/tntn/terra_meshing.h terrainlib/tntn/terra_meshing.cpp
    terrainlib/tntn/TerraUtils.h terrainlib/tntn/TerraUtils.cpp
    terrainlib/tntn/tntn_assert.h
    terrainlib/tntn/util.h terrainlib/tntn/util.cpp
    terrainlib/tntn/ZemlyaMesh.h terrainlib/tntn/ZemlyaMesh.cpp
    terrainlib/tntn/zemlya_meshing.h terrainlib/tntn/zemlya_meshing.cpp
    terrainlib/tntn/ZoomRange.h
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
target_link_libraries(terrainmergerlib PUBLIC terrainlib radix CLI11::CLI11 Eigen3::Eigen CGAL::CGAL)
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
