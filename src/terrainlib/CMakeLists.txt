add_library(terrainlib
    ctb/CTBException.hpp
    ctb/GlobalGeodetic.hpp ctb/GlobalGeodetic.cpp
    ctb/GlobalMercator.hpp ctb/GlobalMercator.cpp
    ctb/Grid.hpp
    ctb/TileCoordinate.hpp
    ctb/types.hpp

    io/bytes.cpp
    io/compression.cpp
    io/utils.cpp

    mesh/connectivity/adjacency.cpp
    mesh/connectivity/boundary.cpp
    mesh/connectivity/manifold.cpp
    mesh/connectivity/topology.cpp
    mesh/connectivity/vertex_index_range.cpp

    mesh/igl/igl.cpp

    mesh/io/gltf.cpp
    mesh/io/terrain.cpp
    mesh/io/texture.cpp
    mesh/io/utils.cpp

    mesh/merging/mapping.cpp

    mesh/bfs.cpp
    mesh/boolean.cpp
    mesh/cleanup.cpp
    mesh/clip.cpp
    mesh/convert.cpp
    mesh/geometry.cpp
    mesh/holes.cpp
    mesh/io.cpp
    mesh/texture_trim.cpp
    mesh/validate.cpp

    octree/Space.cpp
    octree/OddLevelShifted.cpp

    polygon/triangulate.cpp
    polygon/utils.cpp
    polygon/clip.cpp

    texture/dilate_colors.cpp

    uv/unwrap.cpp
    uv/atlas.cpp

    Dataset.cpp
    init.cpp
    log.cpp
    ProgressIndicator.cpp
    tile_path.h
)

target_compile_features(terrainlib PUBLIC cxx_std_23)
target_compile_definitions(terrainlib PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_include_directories(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pch.h)
if(ALP_WARNINGS_AS_ERRORS)
    target_compile_options(terrainlib PUBLIC
        $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Werror>
        $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/WX>
    )
endif()
target_link_libraries(terrainlib PUBLIC
    radix
    ZLIB::ZLIB
    CGAL::CGAL
    Eigen3
    GDAL::GDAL
    spdlog
    fmt
    zpp_bits
    zstd::libzstd_static
    cgltf
    TBB::tbb
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    libassert::assert
    magic_enum::magic_enum
    meshoptimizer
    WideInteger
    igl::core
    igl_restricted::triangle
    xatlas
)

if(ALP_ENABLE_OVERVIEW_READING)
    target_compile_definitions(terrainlib PUBLIC ALP_ENABLE_OVERVIEW_READING)
endif()
