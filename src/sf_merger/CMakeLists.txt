add_library(sfmergerlib INTERFACE)
target_include_directories(sfmergerlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfmergerlib INTERFACE terrainlib)

add_executable(sf-merger
    main.cpp
    cli.cpp
)
target_link_libraries(sf-merger PRIVATE sfmergerlib CLI11::CLI11)
