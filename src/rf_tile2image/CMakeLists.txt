add_library(rftile2imagelib convert.h convert.cpp)
target_include_directories(rftile2imagelib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rftile2imagelib PUBLIC terrainlib)

add_executable(rf-tile2image main.cpp)
target_link_libraries(rf-tile2image PRIVATE rftile2imagelib CLI11::CLI11)
