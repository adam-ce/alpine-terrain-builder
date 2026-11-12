# *****************************************************************************
# AlpineMaps.org
# Copyright (C) 2026 Adam Celarek-Litofcenko
# Copyright (C) 2026 Martin Braunsperger
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
# *****************************************************************************

add_library(terrainlib
    Error.h Error.cpp
    ImageKey.h
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
    macros.h
    opencv_utils.h
    optional_utils.h
    pch.h
    range_utils.h
    string_utils.h
    tile_path.h
    type_utils.h
    variant_utils.h
    vector_utils.h

    cli_support/log.h cli_support/log.cpp
    cli_support/ProgressIndicator.h cli_support/ProgressIndicator.cpp
    cli_support/stop_signal.h cli_support/stop_signal.cpp

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

    geometry/CoordFrame.h
    geometry/PlaneFrame.h
    geometry/Rect.h
    geometry/geometry.h

    gis/Dataset.h gis/Dataset.cpp
    gis/DatasetReader.h gis/DatasetReader.cpp
    gis/gdal.h gis/gdal.cpp
    gis/srs.h gis/srs.cpp
    gis/srs/detail.h gis/srs/detail.cpp
    gis/vector_mask.h gis/vector_mask.cpp
    gis/webmercator.h gis/webmercator.cpp

    io/byte.h io/byte.cpp
    io/compression.h io/compression.cpp
    io/conversion.h
    io/image.h io/image.cpp
    io/envelope.h io/envelope.cpp
    io/glm_serialization.h
    io/hash.h io/hash.cpp
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
    mesh/io.h mesh/io.cpp
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
    octree/storage/snapshot.h
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

    raster/algorithm.h
    raster/View.h
    raster/ClampedView.h
    raster/algorithm/detail.h
    raster/algorithm/conversion.h
    raster/algorithm/reduce.h
    raster/algorithm/reducers.h
    raster/algorithm/scale.h
    raster/algorithm/sample_alignment.h
    raster/algorithm/scaling_detail.h
    raster/algorithm/copy.h
    raster/algorithm/fold.h
    raster/algorithm/transform.h
    raster/algorithm/window_transform.h
    raster/algorithm/zip_transform.h

    raster_store/AttributedRaster.h
    raster_store/StoreTraits.h
    raster_store/TilePool.h
    raster_store/Tile.h
    raster_store/attribution.h raster_store/attribution.cpp
    raster_store/io/ContainerCodec.h raster_store/io/ContainerCodec.cpp
    raster_store/io/SubtileCoder.h
    raster_store/io/TileCodec.h
    raster_store/io/definitions.h
    raster_store/io/float_predictor.h raster_store/io/float_predictor.cpp
    raster_store/io/manifest.h raster_store/io/manifest.cpp
    raster_store/path_layout.h
    raster_store/pixel.h
    raster_store/scaler.h
    raster_store/storage.h

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
    store/inputs.h store/inputs.cpp
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
    store/snapshot.h store/snapshot.cpp
    store/traverse.h

    texture/Mask.h
    texture/dilate_colors.h texture/dilate_colors.cpp

    uv/atlas.h uv/atlas.cpp
    uv/fit.h
    uv/unwrap.h uv/unwrap.cpp
)
set_source_files_properties(
    mesh/io/gltf.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
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
    xatlas
)

# Error captures stack traces; libassert already links cpptrace publicly.
target_link_libraries(terrainlib PRIVATE cpptrace::cpptrace)
# Only io/hash.cpp includes xxHash.
target_link_libraries(terrainlib PRIVATE xxhash)
# Only io/image.cpp includes libjxl and TurboJPEG.
target_link_libraries(terrainlib PRIVATE JpegXL::jxl libjpeg-turbo::turbojpeg-static)
