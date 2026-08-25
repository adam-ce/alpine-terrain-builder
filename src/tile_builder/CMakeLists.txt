add_library(tilebuilderlib
    DatasetReader.h DatasetReader.cpp
    Exception.h
    ParallelTileGenerator.h ParallelTileGenerator.cpp
    ParallelTiler.h ParallelTiler.cpp
    TileHeightsGenerator.h TileHeightsGenerator.cpp
    Tiler.h Tiler.cpp
    TopDownTiler.h TopDownTiler.cpp
    alpine_raster.h alpine_raster.cpp
    depth_first_tile_traverser.h
    image_writer.h image_writer.cpp
)

target_include_directories(tilebuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tilebuilderlib PUBLIC terrainlib ZLIB::ZLIB GDAL::GDAL fmt)

add_executable(tile-builder main.cpp)
target_link_libraries(tile-builder PRIVATE tilebuilderlib)
