if(NOT TARGET Catch2::Catch2)
    alp_add_git_repository(catch2 URL https://github.com/catchorg/Catch2.git COMMITISH v3.8.1)
endif()

function(atb_configure_test target)
    target_compile_definitions(${target} PRIVATE
        CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS
        ALP_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/unittests/data/"
        ALP_UNITTESTS_AUSTRIA_HIGHRES="${ALP_UNITTESTS_AUSTRIA_HIGHRES}"
        ALP_UNITTESTS_EXTENDED=$<BOOL:${ALP_UNITTESTS_EXTENDED}>
        ALP_UNITTESTS_DEBUG_IMAGES=$<BOOL:${ALP_UNITTESTS_DEBUG_IMAGES}>
    )
endfunction()

add_executable(unittests_terrainlib
    catch2_helpers.h
    opencv_helpers.h

    terrainlib/convert.cpp
    terrainlib/cow.cpp
    terrainlib/dataset.cpp
    terrainlib/enumerate.cpp
    terrainlib/fixed_vector.cpp
    terrainlib/grid.cpp
    terrainlib/hash_utils.cpp
    terrainlib/hybrid_index_pair_map.cpp
    terrainlib/hybrid_vector.cpp
    terrainlib/offset_table.cpp
    terrainlib/segmented_buffer.cpp
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
)
target_link_libraries(unittests_terrainlib PRIVATE terrainlib Catch2::Catch2WithMain)
atb_configure_test(unittests_terrainlib)

if(TARGET tilebuilderlib)
    add_executable(unittests_tilebuilder
        catch2_helpers.h
        tilebuilder/alpine_raster_format.cpp
        tilebuilder/dataset_reading.cpp
        tilebuilder/depth_first_tile_traverser.cpp
        tilebuilder/image_writer.cpp
        tilebuilder/parallel_tile_generator.cpp
        tilebuilder/parallel_tiler.cpp
        tilebuilder/tile_heights_generator.cpp
        tilebuilder/top_down_tiler.cpp
    )
    target_link_libraries(unittests_tilebuilder PRIVATE tilebuilderlib Catch2::Catch2WithMain)
    atb_configure_test(unittests_tilebuilder)
endif()

if(TARGET sfbuilderlib)
    add_executable(unittests_sfbuilder
        catch2_helpers.h
        sf_builder/mesh.cpp
        sf_builder/texture.cpp
    )
    target_link_libraries(unittests_sfbuilder PRIVATE sfbuilderlib Catch2::Catch2WithMain)
    atb_configure_test(unittests_sfbuilder)
endif()

if(TARGET tile-downloader)
    add_executable(unittests_tile_downloader
        tile_downloader/logger.cpp
        tile_downloader/url_builder.cpp
    )
    target_include_directories(unittests_tile_downloader PRIVATE ${CMAKE_SOURCE_DIR}/src/tile_downloader)
    target_link_libraries(unittests_tile_downloader PRIVATE terrainlib Catch2::Catch2WithMain)
    atb_configure_test(unittests_tile_downloader)
endif()

if(TARGET dagbuilderlib)
    add_executable(unittests_dagbuilder
        catch2_helpers.h
        dag_builder/fixed_point.cpp
        dag_builder/merge_clusterings.cpp
        dag_builder/pull_reproject_texture.cpp
        dag_builder/slice.cpp
    )
    target_link_libraries(unittests_dagbuilder PRIVATE dagbuilderlib Catch2::Catch2WithMain)
    atb_configure_test(unittests_dagbuilder)
endif()

if(TARGET sfmergerlib)
    add_executable(unittests_sfmerger
        catch2_helpers.h
        sf_merger/sphere_projector.cpp
    )
    target_link_libraries(unittests_sfmerger PRIVATE sfmergerlib Catch2::Catch2WithMain)
    atb_configure_test(unittests_sfmerger)
endif()
