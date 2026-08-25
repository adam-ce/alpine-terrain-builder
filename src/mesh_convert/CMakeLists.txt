add_executable(mesh-convert
    cli.h cli.cpp
    main.cpp
)
target_link_libraries(mesh-convert PRIVATE terrainlib CLI11::CLI11)
