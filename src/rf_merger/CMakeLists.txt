add_library(rfmergerlib
    cli.h cli.cpp
    inputs.h inputs.cpp
    merge.h merge.cpp
    partition.h partition.cpp
    priorities.h priorities.cpp
    produce.h produce.cpp
    selection.h
    statistics.h statistics.cpp
)
target_include_directories(rfmergerlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rfmergerlib PUBLIC terrainlib CLI11::CLI11)

add_executable(rf-merger main.cpp)
target_link_libraries(rf-merger PRIVATE rfmergerlib CLI11::CLI11)
