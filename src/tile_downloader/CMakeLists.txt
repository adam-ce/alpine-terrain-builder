add_executable(tile-downloader
    HttpClient.h
    TileDownloader.h
    TileLogger.h
    TileUrlBuilder.h
    cli.h cli.cpp
    main.cpp
    write_file.h
)

target_link_libraries(tile-downloader PRIVATE terrainlib CLI11::CLI11 CURL::libcurl)
