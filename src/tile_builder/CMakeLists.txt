add_library(tilebuilderlib
    alpine_raster.cpp
    DatasetReader.cpp
    image_writer.cpp
    ParallelTileGenerator.cpp
    ParallelTiler.cpp
    TileHeightsGenerator.cpp
    Tiler.cpp
    TopDownTiler.cpp
)
target_include_directories(tilebuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tilebuilderlib PUBLIC terrainlib ZLIB::ZLIB GDAL::GDAL fmt)

add_executable(tile-builder main.cpp)
target_link_libraries(tile-builder PRIVATE tilebuilderlib)
