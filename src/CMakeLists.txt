cmake_minimum_required(VERSION 3.19)
project(terrain_builder_src LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
# TODO: This is needed to avoid NDEBUG being undefined on Release
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(ALP_ENABLE_ASSERTS ON CACHE BOOL "" FORCE)
endif()

option(ALP_ENABLE_OVERVIEW_READING "enable GDAL overview reading (broken with newer GDAL versions)" OFF)

# ASAN & TSAN
option(ALP_ENABLE_THREAD_SANITIZER "compiles with thread sanitizer enabled" OFF)
option(ALP_ENABLE_ADDRESS_SANITIZER "compiles with address sanitizer enabled" OFF)

if(ALP_ENABLE_ADDRESS_SANITIZER)
    if(ALP_ENABLE_THREAD_SANITIZER)
        message(FATAL_ERROR "Cannot enable both AddressSanitizer and ThreadSanitizer")
    endif()
endif()

if(ALP_ENABLE_ADDRESS_SANITIZER)
    message(STATUS "AddressSanitizer enabled")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(ASAN_FLAGS -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer -g)
        add_compile_options(${ASAN_FLAGS})
        add_link_options(${ASAN_FLAGS})
    else()
        message(WARNING "ALP_ENABLE_ADDRESS_SANITIZER set but compiler is not GCC/Clang; ignoring")
    endif()
endif()

if(ALP_ENABLE_THREAD_SANITIZER)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        message(WARNING "ThreadSanitizer is not recommended with Release builds. "
                        "Use Debug or RelWithDebInfo for accurate race detection.")
    endif()
    message(STATUS "ThreadSanitizer enabled")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(TSAN_FLAGS -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls -g)
        add_compile_options(${TSAN_FLAGS})
        add_link_options(${TSAN_FLAGS})
    else()
        message(WARNING "ALP_ENABLE_THREAD_SANITIZER set but compiler is not GCC/Clang; ignoring")
    endif()
endif()

set(ALP_RELEASE_OPT_LEVEL "3" CACHE STRING "Optimization level (0=O0,1=O1,2=O2,3=O3)")

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Using optimization level ${ALP_RELEASE_OPT_LEVEL}")
    set(CMAKE_CXX_FLAGS_RELEASE "-O${ALP_RELEASE_OPT_LEVEL}")
endif()

find_package(TBB REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

alp_add_git_repository(fmt URL https://github.com/fmtlib/fmt COMMITISH 10.2.1)
alp_add_git_repository(cli11 URL https://github.com/CLIUtils/CLI11.git COMMITISH v2.6.1)

alp_add_git_repository(eigen URL https://gitlab.com/libeigen/eigen.git COMMITISH 3.4.0 DO_NOT_ADD_SUBPROJECT)
add_library(Eigen3 INTERFACE)
target_include_directories(Eigen3 INTERFACE SYSTEM ${eigen_SOURCE_DIR})

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
target_include_directories(cgltf INTERFACE SYSTEM ${cgltf_SOURCE_DIR})

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
alp_add_git_repository(tinygltf URL https://github.com/syoyo/tinygltf COMMITISH 4fea26f6c8652f545560807bccc934cf0cdd86dd)

alp_add_git_repository(stb URL https://github.com/nothings/stb COMMITISH f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe DO_NOT_ADD_SUBPROJECT)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE SYSTEM ${stb_SOURCE_DIR})

alp_add_git_repository(tl_expected URL https://github.com/TartanLlama/expected.git COMMITISH v1.1.0 DO_NOT_ADD_SUBPROJECT)
add_library(tl_expected INTERFACE)
target_include_directories(tl_expected INTERFACE SYSTEM ${tl_expected_SOURCE_DIR}/include)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "SPDLOG_FMT_EXTERNAL" FORCE)
alp_add_git_repository(spdlog URL https://github.com/gabime/spdlog.git COMMITISH v1.13.0)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

alp_add_git_repository(zpp_bits URL https://github.com/eyalz800/zpp_bits.git COMMITISH v4.5 DO_NOT_ADD_SUBPROJECT)
add_library(zpp_bits INTERFACE)
target_include_directories(zpp_bits INTERFACE SYSTEM ${zpp_bits_SOURCE_DIR})

alp_add_git_repository(libassert URL https://github.com/jeremy-rifkin/libassert.git COMMITISH v2.1.0)

alp_add_git_repository(magic_enum URL https://github.com/Neargye/magic_enum.git COMMITISH v0.9.7)

alp_add_git_repository(ftxui URL https://github.com/ArthurSonzogni/FTXUI COMMITISH v6.1.9)

alp_add_git_repository(meshoptimizer URL https://github.com/zeux/meshoptimizer.git COMMITISH v1.0.1)

alp_add_git_repository(wideinteger URL https://github.com/ckormanyos/wide-integer.git COMMITISH 719cbbd557fe974f48b055568e2b149b2444bee9)

alp_add_git_repository(xatlas URL https://github.com/jpcy/xatlas COMMITISH f700c7790aaa030e794b52ba7791a05c085faf0c DO_NOT_ADD_SUBPROJECT)
add_library(xatlas STATIC ${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp)
target_include_directories(xatlas PUBLIC ${xatlas_SOURCE_DIR}/source)

set(LIBIGL_COPYLEFT_CORE ON)
set(LIBIGL_RESTRICTED_TRIANGLE ON)
alp_add_git_repository(libigl URL https://github.com/libigl/libigl.git COMMITISH v2.6.0)

alp_add_git_repository(rectpack2D URL https://github.com/TeamHypersomnia/rectpack2D.git COMMITISH bbc9d0ec05b9b508f6c781667fec718bf9e78d21)

set(METIS_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(METIS_INSTALL OFF CACHE BOOL "" FORCE)
alp_add_git_repository(metis URL https://github.com/scivision/metis COMMITISH v5.2.1.2)
target_compile_definitions(metis INTERFACE IDXTYPEWIDTH=32 REALTYPEWIDTH=32)

alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH 2ce3484513b826bb11e5433f868b048eaffe3e5d NOT_SYSTEM DEEP_CLONE)

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


# Unity Builds
option(ALP_ENABLE_UNITY_BUILD "Enable unity (jumbo) builds to speed compilation" OFF)
set(ALP_UNITY_BUILD_BATCH_SIZE 4 CACHE STRING "Number of source files per unity translation unit")
if(ALP_ENABLE_UNITY_BUILD)
    message(STATUS "ALP_ENABLE_UNITY_BUILD: enabling CMake unity builds (CMAKE_UNITY_BUILD=ON)")
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE ${ALP_UNITY_BUILD_BATCH_SIZE})
endif()


# Own targets
add_library(terrainlib
    terrainlib/ctb/CTBException.hpp
    terrainlib/ctb/GlobalGeodetic.hpp terrainlib/ctb/GlobalGeodetic.cpp
    terrainlib/ctb/GlobalMercator.hpp terrainlib/ctb/GlobalMercator.cpp
    terrainlib/ctb/Grid.hpp
    terrainlib/ctb/TileCoordinate.hpp
    terrainlib/ctb/types.hpp

    terrainlib/io/bytes.cpp
    terrainlib/io/utils.cpp

    terrainlib/mesh/igl/igl.cpp

    terrainlib/mesh/io/gltf.cpp
    terrainlib/mesh/io/terrain.cpp
    terrainlib/mesh/io/texture.cpp
    terrainlib/mesh/io/utils.cpp
    
    terrainlib/mesh/merging/mapping.cpp

    terrainlib/mesh/bfs.cpp
    terrainlib/mesh/boolean.cpp
    terrainlib/mesh/boundary.cpp
    terrainlib/mesh/cleanup.cpp
    terrainlib/mesh/clip.cpp
    terrainlib/mesh/compute_topology.cpp
    terrainlib/mesh/convert.cpp
    terrainlib/mesh/geometry.cpp
    terrainlib/mesh/holes.cpp
    terrainlib/mesh/io.cpp
    terrainlib/mesh/manifold.cpp
    terrainlib/mesh/texture_trim.cpp
    terrainlib/mesh/topology.cpp
    terrainlib/mesh/validate.cpp
    terrainlib/mesh/vertex_index_range.cpp

    terrainlib/octree/storage/helpers.cpp
    terrainlib/octree/IndexMap.cpp
    terrainlib/octree/Space.cpp
    terrainlib/octree/OddLevelShifted.cpp

    terrainlib/polygon/triangulate.cpp 
    terrainlib/polygon/utils.cpp 

    terrainlib/uv/unwrap.cpp

    terrainlib/Dataset.cpp
    terrainlib/init.cpp
    terrainlib/log.cpp
    terrainlib/ProgressIndicator.cpp
)
target_include_directories(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainlib)
target_precompile_headers(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/terrainlib/pch.h)
target_link_libraries(terrainlib PUBLIC 
    radix ZLIB::ZLIB CGAL::CGAL Eigen3 GDAL::GDAL spdlog fmt zpp_bits cgltf TBB::tbb tl_expected opencv_core opencv_imgproc opencv_imgcodecs libassert::assert magic_enum::magic_enum meshoptimizer igl::core igl_restricted::triangle WideInteger)

if(ALP_ENABLE_OVERVIEW_READING)
    target_compile_definitions(terrainlib PUBLIC DATB_ENABLE_OVERVIEW_READING)
endif()


add_library(tilebuilderlib
    tile_builder/alpine_raster.cpp
    tile_builder/DatasetReader.cpp
    tile_builder/Image.cpp
    tile_builder/ParallelTileGenerator.cpp
    tile_builder/ParallelTiler.cpp
    tile_builder/TileHeightsGenerator.cpp
    tile_builder/Tiler.cpp
    tile_builder/TopDownTiler.cpp
)
target_include_directories(tilebuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tile_builder)
target_link_libraries(tilebuilderlib PUBLIC terrainlib radix ZLIB::ZLIB GDAL::GDAL fmt)
add_executable(tile-builder
    tile_builder/main.cpp
)
target_link_libraries(tile-builder PUBLIC tilebuilderlib)


add_library(sfmergerlib sf_merger/merge.h)
target_include_directories(sfmergerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sf_merger)
target_link_libraries(sfmergerlib PUBLIC terrainlib spdlog CLI11::CLI11 tl_expected)
add_executable(sf-merger
    sf_merger/main.cpp
    sf_merger/cli.cpp
)
target_link_libraries(sf-merger PUBLIC sfmergerlib)


add_library(meshbuilderlib
    mesh_builder/terrainbuilder.cpp
    mesh_builder/mesh_builder.cpp
)
target_include_directories(meshbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mesh_builder)
target_link_libraries(meshbuilderlib PUBLIC terrainlib spdlog CLI11::CLI11 tl_expected)
add_executable(mesh-builder
    mesh_builder/main.cpp
)
target_link_libraries(mesh-builder PUBLIC meshbuilderlib)


add_library(meshsimplifylib
    mesh_simplify/cli.cpp
    mesh_simplify/simplify.cpp
    mesh_simplify/uv_map.cpp
)
target_include_directories(meshsimplifylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mesh_simplify)
target_link_libraries(meshsimplifylib PUBLIC terrainlib radix CLI11::CLI11 Eigen3 CGAL::CGAL)
add_executable(mesh-simplify
    mesh_simplify/main.cpp
)
target_link_libraries(mesh-simplify PUBLIC meshsimplifylib)

add_executable(tile-downloader
    tile_downloader/main.cpp
    tile_downloader/cli.cpp
)
target_link_libraries(tile-downloader PUBLIC terrainlib CLI11::CLI11 ${CURL_LIBRARIES})

add_executable(mesh-convert
    mesh_convert/cli.cpp
    mesh_convert/main.cpp
)
target_link_libraries(mesh-convert PUBLIC terrainlib CLI11::CLI11)

add_executable(mesh_merger
    mesh_merger/cli.cpp
    mesh_merger/main.cpp
)
target_link_libraries(mesh_merger PUBLIC terrainlib CLI11::CLI11)

add_executable(index-browser
    index_browser/cli.cpp
    index_browser/main.cpp
)
target_link_libraries(index-browser PUBLIC terrainlib CLI11::CLI11 ftxui::dom ftxui::component ftxui::screen)

add_library(dagbuilderlib
    dag_builder/atlas/Packer.cpp
    dag_builder/atlas/rect/atlas.cpp
    dag_builder/atlas/rect/HorizontalStripPlanner.cpp
    dag_builder/atlas/rect/PackingPlanner.cpp
    dag_builder/build.cpp
    dag_builder/merge/clusterings.cpp
)
target_include_directories(dagbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dag_builder)
target_link_libraries(dagbuilderlib PUBLIC terrainlib libassert::assert metis rectpack2D meshoptimizer xatlas)
add_executable(dag-builder
    dag_builder/cli.cpp
    dag_builder/main.cpp
)
target_link_libraries(dag-builder PUBLIC dagbuilderlib CLI11::CLI11)

add_executable(dag-convert-debug
    dag_convert_debug/cli.cpp
    dag_convert_debug/main.cpp
)
target_link_libraries(dag-convert-debug PUBLIC dagbuilderlib CLI11::CLI11)

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

# Setup compiler caching with sccache or ccache if available
find_program(SCCACHE_PROGRAM sccache)
find_program(CCACHE_PROGRAM ccache)
if (SCCACHE_PROGRAM)
    message(STATUS "Using sccache as compiler launcher: ${SCCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
elseif (CCACHE_PROGRAM)
    message(STATUS "Using ccache as compiler launcher: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()
