add_executable(tile-downloader
    main.cpp
    cli.cpp
)
target_link_libraries(tile-downloader PRIVATE terrainlib CLI11::CLI11 CURL::libcurl)
