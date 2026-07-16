add_library(meshsimplifylib
    cli.cpp
    simplify.cpp
    uv_map.cpp
)
target_include_directories(meshsimplifylib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(meshsimplifylib PUBLIC terrainlib CLI11::CLI11 Eigen3 CGAL::CGAL)

add_executable(mesh-simplify main.cpp)
target_link_libraries(mesh-simplify PRIVATE meshsimplifylib)
