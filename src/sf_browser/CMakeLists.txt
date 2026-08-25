add_subdirectory(res)

add_executable(sf-browser
    main.cpp

    core/Application.h core/Application.cpp
    core/Buffer.h core/Buffer.cpp
    core/Camera.h core/Camera.cpp
    core/geometry/UnitCube.h
    core/rendering/OctreeRenderManager.h core/rendering/OctreeRenderManager.cpp
    core/shader/GLUniformAbstractions.h
    core/shader/Shader.h core/shader/Shader.cpp
    core/shader/ShaderProgram.h core/shader/ShaderProgram.cpp
    core/shader/Uniform.h
    core/window/Window.h core/window/Window.cpp
)

target_include_directories(sf-browser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sf-browser PRIVATE terrainlib glfw glad_gl_core_46 sf-browser-resources imgui)
