add_library(rfbuilderlib
    Mask.h Mask.cpp
    build.h build.cpp
    inputs.h inputs.cpp
    planning.h planning.cpp
)
target_include_directories(rfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rfbuilderlib PUBLIC terrainlib)

add_executable(rf-builder main.cpp)
target_link_libraries(rf-builder PRIVATE rfbuilderlib CLI11::CLI11)
