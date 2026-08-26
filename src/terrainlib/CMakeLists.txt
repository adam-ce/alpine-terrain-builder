add_library(terrainlib
    Error.h Error.cpp
    Dataset.h Dataset.cpp
    ImageKey.h
    ProgressIndicator.h ProgressIndicator.cpp
    Range.h
    SphereProjector.h
    VecHash.h
    VecRange.h
    array_utils.h
    build_config.h
    enumerate.h
    fmt_impls.h
    glm_utils.h
    hash_utils.h
    init.h init.cpp
    log.h log.cpp
    macros.h
    opencv_utils.h
    optional_utils.h
    pch.h
    range_utils.h
    srs.h
    string_utils.h
    tile_path.h
    type_utils.h
    variant_utils.h
    vector_utils.h

    containers/Buffer.h
    containers/Cow.h
    containers/FixedVector.h
    containers/HybridIndexPairMap.h
    containers/HybridVector.h
    containers/OffsetTable.h
    containers/OffsetVector.h
    containers/SegmentedBuffer.h
    containers/Size.h
    containers/StampSet.h
    containers/ThreadLocalCache.h
    containers/TinyVector.h
    containers/UnionFind.h
    containers/Vector2D.h

    ctb/CTBException.hpp
    ctb/GlobalGeodetic.hpp ctb/GlobalGeodetic.cpp
    ctb/GlobalMercator.hpp ctb/GlobalMercator.cpp
    ctb/Grid.hpp
    ctb/TileCoordinate.hpp
    ctb/types.hpp

    geometry/CoordFrame.h
    geometry/PlaneFrame.h
    geometry/Rect.h
    geometry/geometry.h

    io/bytes.h io/bytes.cpp
    io/compression.h io/compression.cpp
    io/conversion.h
    io/envelope.h
    io/envelope_file.h
    io/utils.h io/utils.cpp

    mesh/EncodedMesh.h
    mesh/PointWithMeta.h
    mesh/SimpleMesh.h
    mesh/TriangleContainer.h
    mesh/TriangleSoup.h
    mesh/VertexMap.h
    mesh/View.h
    mesh/WindingOrder.h
    mesh/bfs.h mesh/bfs.cpp
    mesh/boolean.h mesh/boolean.cpp
    mesh/bounds.h
    mesh/cgal.h
    mesh/cleanup.h mesh/cleanup.cpp
    mesh/clip.h mesh/clip.cpp
    mesh/codec/Gltf.h
    mesh/codec/SfMesh.h
    mesh/codec/SfMeshFormat.h
    mesh/codec/from_extension.h
    mesh/combine.h
    mesh/connectivity/adjacency.h mesh/connectivity/adjacency.cpp
    mesh/connectivity/boundary.h mesh/connectivity/boundary.cpp
    mesh/connectivity/connected_components.h
    mesh/connectivity/edges.h
    mesh/connectivity/manifold.h mesh/connectivity/manifold.cpp
    mesh/connectivity/topology.h mesh/connectivity/topology.cpp
    mesh/connectivity/triangle_compare.h
    mesh/connectivity/vertex_index_range.h mesh/connectivity/vertex_index_range.cpp
    mesh/convert.h mesh/convert.cpp
    mesh/encode.h
    mesh/geometry.h mesh/geometry.cpp
    mesh/holes.h mesh/holes.cpp
    mesh/igl/convert.h
    mesh/igl/cut.h
    mesh/igl/cut_to_disk.h
    mesh/igl/igl.cpp
    mesh/igl/manifold.h
    mesh/igl/orient.h
    mesh/io.h mesh/io.cpp
    mesh/io/error.h
    mesh/io/gltf.h mesh/io/gltf.cpp
    mesh/io/options.h
    mesh/io/texture.h mesh/io/texture.cpp
    mesh/io/utils.h mesh/io/utils.cpp
    mesh/merge.h
    mesh/merging/EpsilonVertexDeduplicate.h
    mesh/merging/ExactVertexDeduplicate.h
    mesh/merging/SphereProjectionVertexDeduplicate.h
    mesh/merging/VertexDeduplicate.h
    mesh/merging/VertexId.h
    mesh/merging/VertexMapping.h
    mesh/merging/helpers.h
    mesh/merging/mapping.h mesh/merging/mapping.cpp
    mesh/normalize.h
    mesh/reindex.h
    mesh/split.h
    mesh/storage.h
    mesh/texture_trim.h mesh/texture_trim.cpp
    mesh/utils.h
    mesh/validate.h mesh/validate.cpp
    mesh/validate_cgal.h

    numeric/StrongDouble.h
    numeric/glm_math.h
    numeric/int_math.h
    numeric/number_types.h
    numeric/number_utils.h
    numeric/quantize.h
    numeric/wide_integer.h

    octree/Id.h
    octree/IdRect.h
    octree/OddLevelShifted.h octree/OddLevelShifted.cpp
    octree/Space.h octree/Space.cpp
    octree/StoreTraits.h
    octree/storage/IndexFile.h
    octree/storage/open_runtime.h
    octree/store_layout/Flat.h
    octree/store_layout/LevelAndCoordinateDirectories.h
    octree/store_layout/Mappings.h
    octree/utils.h

    polygon/Polygon.h
    polygon/clip.h polygon/clip.cpp
    polygon/triangulate.h polygon/triangulate.cpp
    polygon/utils.h polygon/utils.cpp

    range/AnyRange.h
    range/Bound.h
    range/Range.h
    range/RangeBounds.h
    range/RangeFrom.h
    range/RangeFull.h
    range/RangeInclusive.h
    range/RangeTo.h
    range/RangeToInclusive.h

    raster_store/StoreTraits.h

    sf/finalize_storage.h
    sf/validate_index.h

    spatial_lookup/CellBased.h
    spatial_lookup/CellBasedStorage.h
    spatial_lookup/Grid.h
    spatial_lookup/GridStorage.h
    spatial_lookup/Hashmap.h
    spatial_lookup/HashmapStorage.h
    spatial_lookup/NDLoopHelper.h
    spatial_lookup/QuantizedHash.h
    spatial_lookup/SpatialLookup.h

    store/Codec.h
    store/Index.h
    store/IndexFormat.h
    store/IndexedStorage.h
    store/NodeStatus.h
    store/NodeStatusOrMissing.h
    store/path_layout.h
    store/RawStorage.h
    store/Storage.h
    store/ThreadSafeStorage.h
    store/Traits.h
    store/cache/Dummy.h
    store/cache/Interface.h
    store/cache/Lru.h
    store/open.h
    store/traverse.h

    texture/Mask.h
    texture/dilate_colors.h texture/dilate_colors.cpp

    uv/atlas.h uv/atlas.cpp
    uv/fit.h
    uv/unwrap.h uv/unwrap.cpp
)
set_source_files_properties(mesh/io/gltf.cpp PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

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
