#include "Application.h"
#include "utils/log/Log.h"
#include <sstream>

Application::Application(std::string title, int width, int height)
    : m_title(title), m_width(width), m_height(height) {

    WindowConfig c = {
        .width = 1280,
        .height = 720,
        .title = "Alpenite Browser",
        .resizeable = true,
        .opengl_version = {4, 6},
        .opengl_core_profile = true
    };

    m_window = std::make_unique<Window>(c);

    init_glad();
    init_gl();
}

void Application::run() {
  while (!m_window->should_close()) {
    // Check if any events have been activated (key pressed, mouse moved etc.)
    // and call corresponding response functions
      m_window->poll_events();
  }
}

Application::~Application() {
  LOG_INFO("Exiting");
}

void Application::init_glad() {
    // Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
    int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
        LOG_GL_FATAL_AND_EXIT("Failed to initialize OpenGL context");
    }

    // Successfully loaded OpenGL
    LOG_GL_INFO("Loaded OpenGL {0}.{1}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
}

void Application::init_gl() {
#ifdef _DEBUG
  glEnable(GL_DEBUG_OUTPUT);
  // set debug callback
  glDebugMessageCallback(gl_debug_callback, nullptr);
#endif // _DEBUG

  glViewport(0, 0, m_width, m_height);
}

void Application::gl_debug_callback(GLenum source, GLenum type,
                                              GLuint id, GLenum severity,
                                              GLsizei length,
                                              const GLchar *message,
                                              const GLvoid *userParam) {
  std::stringstream stringStream;
  std::string sourceString;
  std::string typeString;
  std::string severityString;

  switch (source) {
  case GL_DEBUG_SOURCE_API: {
    sourceString = "API";
    break;
  }
  case GL_DEBUG_SOURCE_APPLICATION: {
    sourceString = "Application";
    break;
  }
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM: {
    sourceString = "Window System";
    break;
  }
  case GL_DEBUG_SOURCE_SHADER_COMPILER: {
    sourceString = "Shader Compiler";
    break;
  }
  case GL_DEBUG_SOURCE_THIRD_PARTY: {
    sourceString = "Third Party";
    break;
  }
  case GL_DEBUG_SOURCE_OTHER: {
    sourceString = "Other";
    break;
  }
  default: {
    sourceString = "Unknown";
    break;
  }
  }

  switch (type) {
  case GL_DEBUG_TYPE_ERROR: {
    typeString = "Error";
    break;
  }
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: {
    typeString = "Deprecated Behavior";
    break;
  }
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: {
    typeString = "Undefined Behavior";
    break;
  }
  case GL_DEBUG_TYPE_PORTABILITY_ARB: {
    typeString = "Portability";
    break;
  }
  case GL_DEBUG_TYPE_PERFORMANCE: {
    typeString = "Performance";
    break;
  }
  case GL_DEBUG_TYPE_OTHER: {
    typeString = "Other";
    break;
  }
  default: {
    typeString = "Unknown";
    break;
  }
  }

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH: {
    severityString = "High";
    break;
  }
  case GL_DEBUG_SEVERITY_MEDIUM: {
    severityString = "Medium";
    break;
  }
  case GL_DEBUG_SEVERITY_LOW: {
    severityString = "Low";
    break;
  }
  case GL_DEBUG_SEVERITY_NOTIFICATION: {
    severityString = "Notification";
    break;
  }
  default: {
    severityString = "Unknown";
    break;
  }
  }

  stringStream << message;
  stringStream << " [Source = " << sourceString;
  stringStream << ", Type = " << typeString;
  stringStream << ", Severity = " << severityString;
  stringStream << ", ID = " << id << "]";

  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH: {
    LOG_GL_ERROR(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_MEDIUM: {
    LOG_GL_WARN(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_LOW: {
    LOG_GL_INFO(stringStream.str());
    break;
  }
  case GL_DEBUG_SEVERITY_NOTIFICATION: {
    LOG_GL_DEBUG(stringStream.str());
    break;
  }
  default: {
    LOG_GL_TRACE(stringStream.str());
    break;
  }
  }
}
