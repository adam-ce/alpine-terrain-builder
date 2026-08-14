set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")

# Dependencies used by the unconditional terrainlib umbrella target.
find_package(ZLIB REQUIRED)

alp_add_git_repository(fmt URL https://github.com/fmtlib/fmt COMMITISH 12.2.0)

alp_add_git_repository(eigen URL https://gitlab.com/libeigen/eigen.git COMMITISH 3.4.0 DO_NOT_ADD_SUBPROJECT)
add_library(Eigen3 INTERFACE)
target_include_directories(Eigen3 SYSTEM INTERFACE ${eigen_SOURCE_DIR})

alp_setup_cmake_project(boost URL https://github.com/boostorg/boost.git COMMITISH "boost-1.88.0" CMAKE_ARGUMENTS -DBOOST_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF "-DBOOST_INCLUDE_LIBRARIES='graph\;multiprecision\;heap\;format\;logic'")
find_package(Boost CONFIG REQUIRED)

alp_setup_cmake_project(tbb URL https://github.com/uxlfoundation/oneTBB.git COMMITISH v2023.1.0 CMAKE_ARGUMENTS -DTBB_TEST=OFF -DTBBMALLOC_BUILD=OFF)
find_package(TBB REQUIRED)

include(../cmake/SetupGDAL.cmake)
alp_setup_gdal(GDAL_VERSION v3.10.3 GEOS_VERSION 3.13.1 PROJ_VERSION 9.6.0)

alp_setup_cmake_project(cgal URL https://github.com/CGAL/cgal.git COMMITISH "v6.0.1" CMAKE_ARGUMENTS -DBUILD_TESTING=OFF -DWITH_examples=OFF)
find_package(CGAL REQUIRED)

include(../cmake/SetupOpenCV.cmake)
alp_setup_opencv(4.11.0)

alp_add_git_repository(cgltf URL https://github.com/AlpineMapsOrgDependencies/cgltf.git COMMITISH e035b820ccde6ab20451b612d8033c9b76ea0ce4 DO_NOT_ADD_SUBPROJECT)
add_library(cgltf INTERFACE)
target_include_directories(cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "" FORCE)
alp_add_git_repository(tinygltf URL https://github.com/syoyo/tinygltf COMMITISH 4fea26f6c8652f545560807bccc934cf0cdd86dd)

alp_add_git_repository(stb URL https://github.com/nothings/stb COMMITISH f4a71b13373436a2866c5d68f8f80ac6f0bc1ffe DO_NOT_ADD_SUBPROJECT)
add_library(stb INTERFACE)
target_include_directories(stb SYSTEM INTERFACE ${stb_SOURCE_DIR})

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "SPDLOG_FMT_EXTERNAL" FORCE)
alp_add_git_repository(spdlog URL https://github.com/gabime/spdlog.git COMMITISH v1.17.0)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

alp_add_git_repository(zpp_bits URL https://github.com/eyalz800/zpp_bits.git COMMITISH v4.7.3 DO_NOT_ADD_SUBPROJECT)
add_library(zpp_bits INTERFACE)
target_include_directories(zpp_bits SYSTEM INTERFACE ${zpp_bits_SOURCE_DIR})

alp_setup_cmake_project(zstd
    URL https://github.com/AlpineMapsOrgDependencies/zstd.git
    COMMITISH 5c7b7bad26808e6b40ac3b3d0075466e27738a9d
    CMAKE_ARGUMENTS
        -DZSTD_BUILD_PROGRAMS=OFF
        -DZSTD_BUILD_TESTS=OFF
        -DZSTD_BUILD_CONTRIB=OFF
        -DZSTD_BUILD_SHARED=OFF
        -DZSTD_BUILD_STATIC=ON
)
find_package(zstd CONFIG REQUIRED)

alp_add_git_repository(libassert URL https://github.com/jeremy-rifkin/libassert.git COMMITISH v2.1.0)
alp_add_git_repository(magic_enum URL https://github.com/Neargye/magic_enum.git COMMITISH v0.9.7)
alp_add_git_repository(meshoptimizer URL https://github.com/zeux/meshoptimizer.git COMMITISH v1.0.1)

alp_add_git_repository(wideinteger URL https://github.com/ckormanyos/wide-integer.git COMMITISH 719cbbd557fe974f48b055568e2b149b2444bee9)

set(LIBIGL_COPYLEFT_CORE ON)
set(LIBIGL_RESTRICTED_TRIANGLE ON)
alp_add_git_repository(libigl URL https://github.com/libigl/libigl.git COMMITISH v2.6.0)

alp_add_git_repository(radix URL https://github.com/AlpineMapsOrg/radix.git COMMITISH 6babc632401cb95a701ce431cb68adf91e2c1b71 NOT_SYSTEM)

set(ALP_CLI_COMPONENT_OPTIONS
    ALP_BUILD_SF_MERGER
    ALP_BUILD_SF_BUILDER
    ALP_BUILD_TILE_DOWNLOADER
    ALP_BUILD_MESH_CONVERT
    ALP_BUILD_SF_INDEX_BROWSER
    ALP_BUILD_DAG_BUILDER
    ALP_BUILD_DAG_CONVERT_DEBUG
)
set(ALP_NEEDS_CLI11 OFF)
foreach(option_name IN LISTS ALP_CLI_COMPONENT_OPTIONS)
    if(${option_name})
        set(ALP_NEEDS_CLI11 ON)
    endif()
endforeach()
if(ALP_NEEDS_CLI11)
    alp_add_git_repository(cli11 URL https://github.com/CLIUtils/CLI11.git COMMITISH v2.6.1)
endif()

if(ALP_BUILD_TILE_DOWNLOADER)
    find_package(CURL REQUIRED)
endif()

if(ALP_BUILD_SF_INDEX_BROWSER)
    alp_add_git_repository(ftxui URL https://github.com/ArthurSonzogni/FTXUI COMMITISH v6.1.9)
endif()

alp_add_git_repository(xatlas URL https://github.com/jpcy/xatlas COMMITISH f700c7790aaa030e794b52ba7791a05c085faf0c DO_NOT_ADD_SUBPROJECT)
add_library(xatlas STATIC ${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp)
target_include_directories(xatlas SYSTEM PUBLIC ${xatlas_SOURCE_DIR}/source)

if(ALP_BUILD_DAG_BUILDER OR ALP_BUILD_DAG_CONVERT_DEBUG)
    alp_add_git_repository(rectpack2D URL https://github.com/TeamHypersomnia/rectpack2D.git COMMITISH bbc9d0ec05b9b508f6c781667fec718bf9e78d21)

    set(METIS_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(METIS_INSTALL OFF CACHE BOOL "" FORCE)
    alp_add_git_repository(metis URL https://github.com/scivision/metis COMMITISH v5.2.1.2)
    target_compile_definitions(metis INTERFACE IDXTYPEWIDTH=32 REALTYPEWIDTH=32)
endif()

if(ALP_BUILD_SF_BROWSER)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    if(UNIX)
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    endif()
    alp_add_git_repository(glfw URL https://github.com/glfw/glfw.git COMMITISH e7ea71be039836da3a98cea55ae5569cb5eb885c)

    alp_add_git_repository(glad URL https://github.com/Dav1dde/glad.git COMMITISH 431786d8126e4f383a81e36f47b61a5d52a1c20d DO_NOT_ADD_SUBPROJECT)
    add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)
    glad_add_library(glad_gl_core_46 REPRODUCIBLE API gl:core=4.6)
    set_property(TARGET glad_gl_core_46 PROPERTY SYSTEM TRUE)

    alp_add_git_repository(resources-lib URL https://github.com/vector-of-bool/cmrc.git COMMITISH 952ffddba731fc110bd50409e8d2b8a06abbd237)

    alp_add_git_repository(imgui URL https://github.com/ocornut/imgui.git COMMITISH 5f0acadf7db1b6d469f0771284e2e3f7818443ce DO_NOT_ADD_SUBPROJECT)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(imgui PUBLIC glfw)
endif()

# First-party components.
if(ALP_ENABLE_UNITY_BUILD)
    message(STATUS "ALP_ENABLE_UNITY_BUILD: enabling unity builds for first-party targets")
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE ${ALP_UNITY_BUILD_BATCH_SIZE})
endif()

add_subdirectory(terrainlib)

if(ALP_BUILD_TILE_BUILDER)
    add_subdirectory(tile_builder)
endif()
if(ALP_BUILD_SF_MERGER)
    add_subdirectory(sf_merger)
endif()
if(ALP_BUILD_SF_BUILDER)
    add_subdirectory(sf_builder)
endif()
if(ALP_BUILD_TILE_DOWNLOADER)
    add_subdirectory(tile_downloader)
endif()
if(ALP_BUILD_MESH_CONVERT)
    add_subdirectory(mesh_convert)
endif()
if(ALP_BUILD_SF_INDEX_BROWSER)
    add_subdirectory(sf_index_browser)
endif()
if(ALP_BUILD_DAG_BUILDER OR ALP_BUILD_DAG_CONVERT_DEBUG)
    add_subdirectory(dag_builder)
endif()
if(ALP_BUILD_DAG_CONVERT_DEBUG)
    add_subdirectory(dag_convert_debug)
endif()
if(ALP_BUILD_SF_BROWSER)
    add_subdirectory(sf_browser)
endif()
