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
  void update_camera(float frame_delta_time, Uniform<glm::mat4> U_view);

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