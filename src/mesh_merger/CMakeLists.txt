add_executable(mesh_merger
    cli.cpp
    main.cpp
)
target_link_libraries(mesh_merger PRIVATE terrainlib CLI11::CLI11)
