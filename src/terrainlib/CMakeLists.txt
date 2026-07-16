add_library(terrainlib
    ctb/CTBException.hpp
    ctb/GlobalGeodetic.hpp ctb/GlobalGeodetic.cpp
    ctb/GlobalMercator.hpp ctb/GlobalMercator.cpp
    ctb/Grid.hpp
    ctb/TileCoordinate.hpp
    ctb/types.hpp

    io/bytes.cpp
    io/utils.cpp

    mesh/igl/igl.cpp

    mesh/io/gltf.cpp
    mesh/io/terrain.cpp
    mesh/io/texture.cpp
    mesh/io/utils.cpp

    mesh/merging/mapping.cpp

    mesh/bfs.cpp
    mesh/boolean.cpp
    mesh/boundary.cpp
    mesh/cleanup.cpp
    mesh/clip.cpp
    mesh/compute_topology.cpp
    mesh/convert.cpp
    mesh/geometry.cpp
    mesh/holes.cpp
    mesh/io.cpp
    mesh/manifold.cpp
    mesh/texture_trim.cpp
    mesh/topology.cpp
    mesh/validate.cpp
    mesh/vertex_index_range.cpp

    octree/storage/helpers.cpp
    octree/IndexMap.cpp
    octree/Space.cpp
    octree/OddLevelShifted.cpp

    polygon/triangulate.cpp
    polygon/utils.cpp

    uv/unwrap.cpp

    Dataset.cpp
    init.cpp
    log.cpp
    ProgressIndicator.cpp
)

target_compile_features(terrainlib PUBLIC cxx_std_20)
target_compile_definitions(terrainlib PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_include_directories(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_precompile_headers(terrainlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pch.h)
target_link_libraries(terrainlib PUBLIC
    radix
    ZLIB::ZLIB
    CGAL::CGAL
    Eigen3
    GDAL::GDAL
    spdlog
    fmt
    zpp_bits
    cgltf
    TBB::tbb
    tl_expected
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    libassert::assert
    magic_enum::magic_enum
    meshoptimizer
    igl::core
    igl_restricted::triangle
)

if(ALP_ENABLE_OVERVIEW_READING)
    target_compile_definitions(terrainlib PUBLIC ALP_ENABLE_OVERVIEW_READING)
endif()
