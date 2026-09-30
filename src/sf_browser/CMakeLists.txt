# *****************************************************************************
# AlpineMaps.org
# Copyright (C) 2023 Adam Celarek-Litofcenko
# Copyright (C) 2023 Martin Braunsperger
# Copyright (C) 2023 Adrian Gawor
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# *****************************************************************************

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
