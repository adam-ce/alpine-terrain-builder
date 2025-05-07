include(FetchContent)

if(NOT TARGET glm)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG master
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/glm
        GIT_PROGRESS TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )
    message(STATUS "Including glm")
    FetchContent_MakeAvailable(glm)
endif()

if(NOT TARGET glfw)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG master
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/glfw
        GIT_PROGRESS TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )

    # Don't build glfw docs, tests and examples
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    
    if (UNIX)
        # Disable Wayland and enable X11
        set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    endif (UNIX)
    
    message(STATUS "Including glfw")
    FetchContent_MakeAvailable(glfw)
endif()

if(NOT TARGET glad_gl_core_46)
    FetchContent_Declare(glad
        GIT_REPOSITORY https://github.com/Dav1dde/glad
        GIT_TAG glad2
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/glad
        GIT_PROGRESS TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )
    message(STATUS "Including glad")
    FetchContent_MakeAvailable(glad)

    # Generate glad/gl.h for OpenGL 4.6
    set(GLAD_SOURCES_DIR "${PROJECT_SOURCE_DIR}/3rdparty/glad")
    add_subdirectory("${GLAD_SOURCES_DIR}/cmake" glad_cmake)
    glad_add_library(glad_gl_core_46 REPRODUCIBLE API gl:core=4.6)
endif()

if(NOT TARGET resources-lib)
    FetchContent_Declare(CMakeRC
        GIT_REPOSITORY https://github.com/vector-of-bool/cmrc.git
        GIT_TAG master
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/CMakeRC
        GIT_PROGRESS TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )
    message(STATUS "Including CMakeRC")
    FetchContent_MakeAvailable(CMakeRC)

    add_subdirectory(res)
endif()

if(NOT TARGET spdlog)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.x
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/spdlog
        GIT_PROGRESS TRUE
        USES_TERMINAL_DOWNLOAD TRUE
    )
    message(STATUS "Including spdlog")
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT TARGET radix)
    FetchContent_Declare(radix
        GIT_REPOSITORY https://github.com/AlpineMapsOrg/radix.git
        GIT_TAG origin/main
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/radix
        SOURCE_SUBDIR src
    )
    message(STATUS "Including radix")
    FetchContent_MakeAvailable(radix)
endif()

add_executable(alpenite-browser
    main.cpp

    core/Application.cpp
    core/window/Window.cpp
    core/shader/Shader.cpp
    core/shader/ShaderProgram.cpp
    core/shader/Uniform.h
    core/shader/GLUniformAbstractions.h
    core/Buffer.cpp
    core/Camera.cpp

    utils/log/Log.cpp

    core/geometry/UnitCube.h

    core/octree/id.h
    core/octree/space.h
)
target_link_libraries(alpenite-browser PRIVATE glad_gl_core_46 glfw glm resources-lib spdlog radix)
target_include_directories(alpenite-browser PRIVATE .)