add_library(meshbuilderlib
    terrainbuilder.cpp
    mesh_builder.cpp
)
target_include_directories(meshbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meshbuilderlib PUBLIC terrainlib spdlog tl_expected)

add_executable(mesh-builder main.cpp)
target_link_libraries(mesh-builder PRIVATE meshbuilderlib CLI11::CLI11)
