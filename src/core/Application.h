#pragma once
#include <glad/gl.h>

#include <GLFW/glfw3.h>

#include <string>
#include <memory>
#include "Window.h"

class Application {
public:
  Application(std::string title, int width, int height);

  void run();

  ~Application();

private:
  std::string m_title;
  int m_width, m_height;

  std::unique_ptr<Window> m_window;

  void init_glad();
  void init_gl();

  static void gl_debug_callback(GLenum source, GLenum type, GLuint id,
                                          GLenum severity, GLsizei length,
                                          const GLchar *message,
                                          const GLvoid *userParam);
};