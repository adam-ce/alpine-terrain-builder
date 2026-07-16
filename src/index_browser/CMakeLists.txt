add_executable(index-browser
    cli.cpp
    main.cpp
)
target_link_libraries(index-browser PRIVATE terrainlib CLI11::CLI11 ftxui::dom ftxui::component ftxui::screen)
