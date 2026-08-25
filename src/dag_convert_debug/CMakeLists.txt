add_executable(dag-convert-debug
    cli.h cli.cpp
    main.cpp
)
target_link_libraries(dag-convert-debug PRIVATE dagbuilderlib CLI11::CLI11)
