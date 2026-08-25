add_library(sfbuilderlib
    border.h
    mesh_builder.h mesh_builder.cpp
    raw_dataset_reader.h
    terrainbuilder.h terrainbuilder.cpp
    texture_assembler.h
    tile_provider.h
)

target_include_directories(sfbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfbuilderlib PUBLIC terrainlib spdlog)

add_executable(sf-builder main.cpp)
target_link_libraries(sf-builder PRIVATE sfbuilderlib CLI11::CLI11)
