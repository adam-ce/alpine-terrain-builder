cmake_minimum_required(VERSION 3.21)

project(alpine-terrain-builder-unittests)

option(ATB_UNITTESTS_EXTENDED "perform extended unit tests (they take long)" OFF)
option(ATB_UNITTESTS_DEBUG_IMAGES "output debug height images for visual comparison" OFF)
set(ATB_UNITTESTS_AUSTRIA_HIGHRES CACHE FILEPATH "path to the high res austrian dataset (terrain model, i.e., OeRect_01m_gt_31287.img)")

if (NOT TARGET Catch2)
    alp_add_git_repository(catch2 URL https://github.com/catchorg/Catch2.git COMMITISH v3.5.1)
endif()

add_executable(unittests_terrainlib
    alpine_raster_format.cpp
    catch2_helpers.h

    dataset.cpp
    dataset_reading.cpp
    depth_first_tile_traverser.cpp
    grid.cpp
    image.cpp
    main.cpp
    parallel_tile_generator.cpp
    parallel_tiler.cpp
    progress_indicator_test.cpp
    srs_test.cpp
    tile_heights_generator.cpp
    top_down_tiler.cpp
)
    
target_compile_definitions(unittests_terrainlib PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
target_compile_definitions(unittests_terrainlib PUBLIC "ATB_UNITTESTS_AUSTRIA_HIGHRES=\"${ATB_UNITTESTS_AUSTRIA_HIGHRES}\"")
if (ATB_UNITTESTS_EXTENDED)
    target_compile_definitions(unittests_terrainlib PUBLIC "ATB_UNITTESTS_EXTENDED=true")
else()
    target_compile_definitions(unittests_terrainlib PUBLIC "ATB_UNITTESTS_EXTENDED=false")
endif()
if (ATB_UNITTESTS_DEBUG_IMAGES)
    target_compile_definitions(unittests_terrainlib PUBLIC "ATB_UNITTESTS_DEBUG_IMAGES=true")
else()
    target_compile_definitions(unittests_terrainlib PUBLIC "ATB_UNITTESTS_DEBUG_IMAGES=false")
endif()
target_link_libraries(unittests_terrainlib PUBLIC terrainlib Catch2::Catch2WithMain)


add_executable(unittests_terrainmerger
    catch2_helpers.h
    terrainmerger/convert.cpp
    terrainmerger/merge.cpp
)

target_link_libraries(unittests_terrainmerger PUBLIC terrainmergerlib Catch2::Catch2WithMain)


add_executable(unittests_terrainbuilder
    catch2_helpers.h
    terrainbuilder/octree_id.cpp
    terrainbuilder/octree_space.cpp
    terrainbuilder/mesh2.cpp
    # TODO:
    # terrainbuilder/mesh_io.cpp
    # terrainbuilder/mesh.cpp
    # terrainbuilder/texture.cpp
)

target_link_libraries(unittests_terrainbuilder PUBLIC terrainbuilderlib terrainmergerlib terrainlib Catch2WithMain CGAL::CGAL)
target_compile_definitions(unittests_terrainbuilder PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
