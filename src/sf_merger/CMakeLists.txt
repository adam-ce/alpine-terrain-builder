add_library(sfmergerlib INTERFACE
    NodeLoader.h
    NodeWriter.h
    cut.h
    earth.h
    mask.h
    merge.h
    utils.h

    atlas/HorizontalStripLayoutPlanner.h
    atlas/Layout.h
    atlas/LayoutPlanner.h
    atlas/Sheet.h
    atlas/build.h

    merge/NodeData.h
    merge/Result.h
    merge/visitor/Masked.h
    merge/visitor/Simple.h
    merge/visitor/Visitor.h
)

target_include_directories(sfmergerlib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sfmergerlib INTERFACE terrainlib)

add_executable(sf-merger
    cli.h cli.cpp
    main.cpp
)
target_link_libraries(sf-merger PRIVATE sfmergerlib CLI11::CLI11)
