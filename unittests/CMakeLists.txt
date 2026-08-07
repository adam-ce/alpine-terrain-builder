cmake_minimum_required(VERSION 3.21)

project(alpine-terrain-builder-unittests)

option(ATB_UNITTESTS_EXTENDED "perform extended unit tests (they take long)" OFF)
option(ATB_UNITTESTS_DEBUG_IMAGES "output debug height images for visual comparison" OFF)
set(ATB_UNITTESTS_AUSTRIA_HIGHRES CACHE FILEPATH "path to the high res austrian dataset (terrain model, i.e., OeRect_01m_gt_31287.img)")

add_compile_definitions(CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS)
if (NOT TARGET Catch2)
    alp_add_git_repository(catch2 URL https://github.com/catchorg/Catch2.git COMMITISH v3.8.1)
endif()


add_compile_definitions(
    ATB_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/unittests/data/"
    ATB_UNITTESTS_AUSTRIA_HIGHRES="${ATB_UNITTESTS_AUSTRIA_HIGHRES}"
)

if (ATB_UNITTESTS_EXTENDED)
    add_compile_definitions(ATB_UNITTESTS_EXTENDED=true)
else()
    add_compile_definitions(ATB_UNITTESTS_EXTENDED=false)
endif()

if (ATB_UNITTESTS_DEBUG_IMAGES)
    add_compile_definitions(ATB_UNITTESTS_DEBUG_IMAGES=true)
else()
    add_compile_definitions(ATB_UNITTESTS_DEBUG_IMAGES=false)
endif()

add_executable(unittests_terrainlib
    catch2_helpers.h
    opencv_helpers.h
    
    terrainlib/convert.cpp
    terrainlib/cow.cpp
    terrainlib/dataset.cpp
    terrainlib/enumerate.cpp
    terrainlib/fixed_vector.cpp
    terrainlib/geometry_frames.cpp
    terrainlib/grid.cpp
    terrainlib/hash_utils.cpp
    terrainlib/hybrid_index_pair_map.cpp
    terrainlib/hybrid_vector.cpp
    terrainlib/offset_table.cpp
    terrainlib/segmented_buffer.cpp
    terrainlib/stamp_set.cpp
    terrainlib/index_map.cpp
    terrainlib/index_traverse.cpp
    terrainlib/mesh_boundary.cpp
    terrainlib/mesh_bounds.cpp
    terrainlib/mesh_cleanup.cpp
    terrainlib/mesh_clip.cpp
    terrainlib/mesh_combine.cpp
    terrainlib/mesh_connected_components.cpp
    terrainlib/mesh_cut.cpp
    terrainlib/mesh_cut_to_disk.cpp
    terrainlib/mesh_holes.cpp
    terrainlib/mesh_io.cpp
    terrainlib/mesh_manifold.cpp
    terrainlib/mesh_merge_mapping.cpp
    terrainlib/mesh_merge.cpp
    terrainlib/mesh_reindex.cpp
    terrainlib/mesh_split.cpp
    terrainlib/mesh_topology.cpp
    terrainlib/octree_id.cpp
    terrainlib/octree_space.cpp
    terrainlib/octree_odd_level_shifted.cpp
    terrainlib/polygon_triangulate.cpp
    terrainlib/progress_indicator_test.cpp
    terrainlib/quantize.cpp
    terrainlib/range.cpp
    terrainlib/range_utils.cpp
    terrainlib/spatial_lookup.cpp
    terrainlib/string_utils.cpp
    terrainlib/tiny_vector.cpp
    terrainlib/union_find.cpp
    terrainlib/uv_atlas.cpp
)
target_link_libraries(unittests_terrainlib PUBLIC terrainlib Catch2::Catch2WithMain)


add_executable(unittests_tilebuilder
    catch2_helpers.h
    tilebuilder/alpine_raster_format.cpp
    tilebuilder/dataset_reading.cpp
    tilebuilder/depth_first_tile_traverser.cpp
    tilebuilder/image.cpp
    tilebuilder/parallel_tile_generator.cpp
    tilebuilder/parallel_tiler.cpp
    tilebuilder/tile_heights_generator.cpp
    tilebuilder/top_down_tiler.cpp
)  
target_link_libraries(unittests_tilebuilder PUBLIC tilebuilderlib Catch2::Catch2WithMain)

add_executable(unittests_meshbuilder
    catch2_helpers.h
    mesh_builder/mesh.cpp
    mesh_builder/texture.cpp
)
target_link_libraries(unittests_meshbuilder PUBLIC meshbuilderlib terrainlib Catch2WithMain)
target_compile_definitions(unittests_meshbuilder PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")


add_executable(unittests_dagbuilder
    catch2_helpers.h
    dag_builder/correspondence.cpp
    dag_builder/merge_clusterings.cpp
    dag_builder/weld.cpp
    dag_builder/slice.cpp
    dag_builder/pull_reproject_texture.cpp
    dag_builder/fixed_point.cpp
)
target_link_libraries(unittests_dagbuilder PUBLIC dagbuilderlib Catch2::Catch2WithMain)

add_executable(unittests_sfmerger
    catch2_helpers.h
    sf_merger/sphere_projector.cpp
)
target_link_libraries(unittests_sfmerger PUBLIC sfmergerlib terrainlib Catch2WithMain)
