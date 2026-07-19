add_library(sfbuilderlib
    terrainbuilder.cpp
    mesh_builder.cpp
)
target_include_directories(sfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfbuilderlib PUBLIC terrainlib spdlog)

add_executable(sf-builder main.cpp)
target_link_libraries(sf-builder PRIVATE sfbuilderlib CLI11::CLI11)
