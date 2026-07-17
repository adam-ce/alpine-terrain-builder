add_library(dagbuilderlib
    atlas/Packer.cpp
    atlas/rect/atlas.cpp
    atlas/rect/HorizontalStripPlanner.cpp
    atlas/rect/PackingPlanner.cpp
    build.cpp
    merge/clusterings.cpp
)
target_include_directories(dagbuilderlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dagbuilderlib PUBLIC terrainlib libassert::assert metis rectpack2D meshoptimizer xatlas)

if(ALP_BUILD_DAG_BUILDER)
    add_executable(dag-builder
        cli.cpp
        main.cpp
    )
    target_link_libraries(dag-builder PRIVATE dagbuilderlib CLI11::CLI11)
endif()
