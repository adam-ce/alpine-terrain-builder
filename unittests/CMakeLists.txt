cmake_minimum_required(VERSION 3.21)

project(alpine-terrain-builder-unittests)

option(ATB_UNITTESTS_EXTENDED "perform extended unit tests (they take long)" OFF)
option(ATB_UNITTESTS_DEBUG_IMAGES "output debug height images for visual comparison" OFF)
set(ATB_UNITTESTS_AUSTRIA_HIGHRES CACHE FILEPATH "path to the high res austrian dataset (terrain model, i.e., OeRect_01m_gt_31287.img)")

add_compile_definitions(CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS)
if (NOT TARGET Catch2)
    alp_add_git_repository(catch2 URL https://github.com/catchorg/Catch2.git COMMITISH v3.5.1)
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
    terrainlib/convert.cpp
    terrainlib/dataset.cpp
    terrainlib/grid.cpp
    terrainlib/progress_indicator_test.cpp
    terrainlib/index_map.cpp
    terrainlib/index_traverse.cpp
    terrainlib/octree_id.cpp
    terrainlib/octree_space.cpp
    terrainlib/mesh_io.cpp
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


add_executable(unittests_terrainsimplify
    catch2_helpers.h
    terrainsimplify/merge.cpp
)

target_link_libraries(unittests_terrainsimplify PUBLIC terrainsimplifylib Catch2::Catch2WithMain)


add_executable(unittests_terrainbuilder
    catch2_helpers.h
    terrainbuilder/mesh.cpp
    terrainbuilder/texture.cpp
)

target_link_libraries(unittests_terrainbuilder PUBLIC terrainbuilderlib terrainsimplifylib terrainlib Catch2WithMain)
target_compile_definitions(unittests_terrainbuilder PUBLIC "ATB_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/unittests/data/\"")
