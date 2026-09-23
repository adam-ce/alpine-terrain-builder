add_library(rfbuilderlib
    Mask.h Mask.cpp
    gdal/TileWorker.h TilePool.h
    run.h run.cpp
    tiles/provider.h tiles/provider.cpp
    tiles/HttpClient.h tiles/HttpClient.cpp
    tiles/jpeg.h tiles/jpeg.cpp
    tiles/planning.h tiles/planning.cpp
    tiles/inputs.h tiles/inputs.cpp
    tiles/TileWorker.h tiles/TileWorker.cpp
    tiles/build.h tiles/build.cpp
    tiles/cli.h tiles/cli.cpp
    gdal/cli.h gdal/cli.cpp
    gdal/build.h gdal/build.cpp
    gdal/inputs.h gdal/inputs.cpp
    gdal/planning.h gdal/planning.cpp
    gdal/nodata.h gdal/nodata.cpp
)
target_include_directories(rfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rfbuilderlib PUBLIC terrainlib CLI11::CLI11 CURL::libcurl)

add_executable(rf-builder main.cpp)
target_link_libraries(rf-builder PRIVATE rfbuilderlib CLI11::CLI11)
