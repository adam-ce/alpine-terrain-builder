/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adrian Gawor
 * Copyright (C) 2025 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <string>
#include <memory>
#include "window/Window.h"
#include "Camera.h"
#include "shader/Uniform.h"

class Application {
public:
  Application(std::string title, int width, int height);

  void run();
  void update_camera(float frame_delta_time);

  ~Application();

private:
  std::string m_title;
  int m_width, m_height;

  std::unique_ptr<Window> m_window;
  std::unique_ptr<Camera> m_camera;

  float m_movement_speed, m_roll_speed, m_mouse_sensitivity;

  bool m_nav_mode;

  float m_refining_factor;

  size_t m_last_draw_amount;

  void toggle_nav_mode();

  void init_glad();
  void init_gl();

  void draw_settings_window();
  void draw_camera_settings_section();
  void draw_octree_settings_section();

  static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                                          GLenum severity, GLsizei length,
                                          const GLchar *message,
                                          const GLvoid *userParam);
};
