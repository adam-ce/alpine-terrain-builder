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

alp_setup_cmake_project(boost URL https://github.com/boostorg/boost.git COMMITISH "boost-1.88.0" CMAKE_ARGUMENTS -DBOOST_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF "-DBOOST_INCLUDE_LIBRARIES='graph\;multiprecision\;heap\;format'")
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

# Don't build glfw docs, tests and examples
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (UNIX)
	# Enable Wayland and X11 on Unix builds
	set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
	set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
endif (UNIX)
alp_add_git_repository(glfw URL https://github.com/glfw/glfw.git COMMITISH e7ea71be039836da3a98cea55ae5569cb5eb885c NOT_SYSTEM)

alp_add_git_repository(glad URL https://github.com/Dav1dde/glad.git COMMITISH 431786d8126e4f383a81e36f47b61a5d52a1c20d DO_NOT_ADD_SUBPROJECT)
add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)
glad_add_library(glad_gl_core_46 REPRODUCIBLE API gl:core=4.6)

alp_add_git_repository(resources-lib URL https://github.com/vector-of-bool/cmrc.git COMMITISH 952ffddba731fc110bd50409e8d2b8a06abbd237 NOT_SYSTEM)
add_subdirectory(browser/res resources-lib)

alp_add_git_repository(imgui URL https://github.com/ocornut/imgui.git COMMITISH 5f0acadf7db1b6d469f0771284e2e3f7818443ce DO_NOT_ADD_SUBPROJECT)
add_library(imgui STATIC
   ${imgui_SOURCE_DIR}/imgui.cpp
   ${imgui_SOURCE_DIR}/imgui_draw.cpp
   ${imgui_SOURCE_DIR}/imgui_tables.cpp
   ${imgui_SOURCE_DIR}/imgui_widgets.cpp
   ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
   ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
   ${imgui_SOURCE_DIR}
   ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

add_library(terrainlib
    terrainlib/ctb/CTBException.hpp
    terrainlib/ctb/GlobalGeodetic.hpp terrainlib/ctb/GlobalGeodetic.cpp
    terrainlib/ctb/GlobalMercator.hpp terrainlib/ctb/GlobalMercator.cpp
    terrainlib/ctb/Grid.hpp
    terrainlib/ctb/TileCoordinate.hpp
    terrainlib/ctb/types.hpp

    terrainlib/io/bytes.h terrainlib/io/bytes.cpp
    terrainlib/io/Error.h
    terrainlib/io/serialize.h
    terrainlib/io/utils.h terrainlib/io/utils.cpp

    terrainlib/mesh/io/error.h
    terrainlib/mesh/io/gltf.h terrainlib/mesh/io/gltf.cpp
    terrainlib/mesh/io/options.h
    terrainlib/mesh/io/terrain.h terrainlib/mesh/io/terrain.cpp
    terrainlib/mesh/io/texture.h terrainlib/mesh/io/texture.cpp
    terrainlib/mesh/io/utils.h terrainlib/mesh/io/utils.cpp

    terrainlib/mesh/cgal.h
    terrainlib/mesh/convert.h
    terrainlib/mesh/convert.cpp
    terrainlib/mesh/io.h terrainlib/mesh/io.cpp
    terrainlib/mesh/SimpleMesh.h
    terrainlib/mesh/utils.h terrainlib/mesh/utils.cpp

    terrainlib/octree/disk/layout/strategy/Flat.h
    terrainlib/octree/disk/layout/strategy/LevelAndCoordinateDirectories.h
    terrainlib/octree/disk/layout/Strategy.h
    terrainlib/octree/disk/layout/StrategyRegister.h
    terrainlib/octree/disk/IndexFile.h
    terrainlib/octree/disk/Layout.h
    terrainlib/octree/Id.h
    terrainlib/octree/Storage.h terrainlib/octree/Storage.cpp
    terrainlib/octree/IndexMap.h terrainlib/octree/IndexMap.cpp
    terrainlib/octree/NodeStatus.h
    terrainlib/octree/Space.h terrainlib/octree/Space.cpp
    terrainlib/octree/traverse.h
    terrainlib/octree/utils.h

    terrainlib/Dataset.h terrainlib/Dataset.cpp
    terrainlib/hash_utils.h
    terrainlib/init.h terrainlib/init.cpp
    terrainlib/log.h terrainlib/log.cpp
    terrainlib/ProgressIndicator.h terrainlib/ProgressIndicator.cpp
    terrainlib/srs.h
    terrainlib/string_utils.h
)
target_include_directories(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainlib)
target_link_libraries(terrainlib PUBLIC radix ZLIB::ZLIB CGAL::CGAL GDAL::GDAL spdlog fmt zpp_bits cgltf TBB::tbb tl_expected opencv_core opencv_imgproc opencv_imgcodecs)

if(ALP_ENABLE_OVERVIEW_READING)
    target_compile_definitions(terrainlib PUBLIC DATB_ENABLE_OVERVIEW_READING)
endif()


add_library(tilebuilderlib
    tilebuilder/alpine_raster.h tilebuilder/alpine_raster.cpp
    tilebuilder/DatasetReader.h tilebuilder/DatasetReader.cpp
    tilebuilder/depth_first_tile_traverser.h
    tilebuilder/Exception.h
    tilebuilder/Image.h tilebuilder/Image.cpp
    tilebuilder/ParallelTileGenerator.h tilebuilder/ParallelTileGenerator.cpp
    tilebuilder/ParallelTiler.h tilebuilder/ParallelTiler.cpp
    tilebuilder/srs.h
    tilebuilder/TileHeightsGenerator.h tilebuilder/TileHeightsGenerator.cpp
    tilebuilder/Tiler.h tilebuilder/Tiler.cpp
    tilebuilder/TopDownTiler.h tilebuilder/TopDownTiler.cpp
    tilebuilder/algorithms/primitives.h
    tilebuilder/algorithms/raster_triangle_scanline.h
)
target_include_directories(tilebuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tilebuilder)
target_link_libraries(tilebuilderlib PUBLIC terrainlib radix ZLIB::ZLIB GDAL::GDAL fmt)
add_executable(tilebuilder
    tilebuilder/main.cpp
)
target_link_libraries(tilebuilder PUBLIC tilebuilderlib)


add_library(terrainmergerlib
    terrainmerger/merge.h
)
target_include_directories(terrainmergerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainmerger)
target_link_libraries(terrainmergerlib PUBLIC terrainlib spdlog CLI11::CLI11 tl_expected)
add_executable(terrainmerger
    terrainmerger/main.cpp
    terrainmerger/cli.h terrainmerger/cli.cpp
)
target_link_libraries(terrainmerger PUBLIC terrainmergerlib)


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


add_library(terrainsimplifylib
    terrainsimplify/cli.h
    terrainsimplify/cli.cpp
    terrainsimplify/merge.h
    terrainsimplify/merge.cpp
    terrainsimplify/simplify.h
    terrainsimplify/simplify.cpp
    terrainsimplify/uv_map.h
    terrainsimplify/uv_map.cpp
    terrainsimplify/validate.h
)
target_include_directories(terrainsimplifylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainsimplify)
target_precompile_headers(terrainsimplifylib PRIVATE terrainsimplify/pch.h)
target_link_libraries(terrainsimplifylib PUBLIC terrainlib radix CLI11::CLI11 Eigen3 CGAL::CGAL)
add_executable(terrainsimplify
    terrainsimplify/main.cpp
)
target_link_libraries(terrainsimplify PUBLIC terrainsimplifylib)

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

add_executable(browser
    # browser/main2.cpp

    browser/main.cpp

    browser/core/Application.cpp
    browser/core/window/Window.cpp
    browser/core/shader/Shader.cpp
    browser/core/shader/ShaderProgram.cpp
    browser/core/shader/Uniform.h
    browser/core/shader/GLUniformAbstractions.h
    browser/core/Buffer.cpp
    browser/core/Camera.cpp

    browser/core/geometry/UnitCube.h

    browser/core/rendering/OctreeRenderManager.cpp
)

#target_compile_features(browser PRIVATE cxx_std_20)
#target_compile_options(browser PRIVATE /utf-8)
target_include_directories(browser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/browser)
target_link_libraries(browser PUBLIC terrainlib glfw glad_gl_core_46 resources-lib imgui)