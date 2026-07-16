add_subdirectory(res)

add_executable(browser
    main.cpp
    core/Application.cpp
    core/window/Window.cpp
    core/shader/Shader.cpp
    core/shader/ShaderProgram.cpp
    core/shader/Uniform.h
    core/shader/GLUniformAbstractions.h
    core/Buffer.cpp
    core/Camera.cpp
    core/geometry/UnitCube.h
    core/rendering/OctreeRenderManager.cpp
)
target_include_directories(browser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(browser PRIVATE terrainlib glfw glad_gl_core_46 resources-lib imgui)
