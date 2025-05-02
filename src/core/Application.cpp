#include "Application.h"
#include "utils/log/Log.h"
#include <sstream>
#include <glm/glm.hpp>

// CMRC Resource Compiler
#include <cmrc/cmrc.hpp>
#include "Shader.h"
#include "ShaderProgram.h"
#include "Buffer.h"
#include "Camera.h"

CMRC_DECLARE(res);

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
    auto RES = cmrc::res::get_filesystem();

    double last_frame_time = glfwGetTime();

    LOG_INFO("Setting up shaders");
    auto vertex_shader_code = RES.open("shaders/basic.vert");
    Shader vertex_shader(GL_VERTEX_SHADER);
    vertex_shader.compile(std::string_view(vertex_shader_code.begin(), vertex_shader_code.end()));

    auto fragment_shader_code = RES.open("shaders/basic.frag");
    Shader fragment_shader(GL_FRAGMENT_SHADER);
    fragment_shader.compile(std::string_view(fragment_shader_code.begin(), fragment_shader_code.end()));

    ShaderProgram shader_program;

    shader_program.attach(vertex_shader);
    shader_program.attach(fragment_shader);
    shader_program.link();
    shader_program.use();

    Uniform<glm::mat4> U_projection = shader_program.get_uniform<glm::mat4>("projection");
    Uniform<glm::mat4> U_view = shader_program.get_uniform<glm::mat4>("view");

    LOG_INFO("Setting up camera");
    CameraConfig camera_config = {
        .fov_deg = 60.0f,
        .aspect_ratio = m_window->getAspectRatio(),
        .near_plane = 0.1f,
        .far_plane = 100.0f,

        .position = glm::vec3(0.0f, 0.0f, 5.0f),
        .target = glm::vec3(0.0f),
        .up = glm::vec3(0.0f, 1.0f, 0.0f),
    };
    Camera camera(camera_config);

    U_projection.set(camera.projection_matrix());
    U_view.set(camera.view_matrix());

    LOG_INFO("Setting up lines");

    std::vector<glm::vec3> vertices = {
        glm::vec3(-1.0, -1.0, -1.0), //000  //#0
        glm::vec3(-1.0,  1.0, -1.0), //010  //#1
        glm::vec3( 1.0,  1.0, -1.0), //110  //#2
        glm::vec3( 1.0, -1.0, -1.0), //100  //#3

        glm::vec3(-1.0, -1.0, 1.0), //001   //#4
        glm::vec3(-1.0,  1.0, 1.0), //011   //#5
        glm::vec3( 1.0,  1.0, 1.0), //111   //#6
        glm::vec3( 1.0, -1.0, 1.0), //101   //#7
    };

    std::vector<unsigned int> indices = {
        //Bottom Loop
        0, 1, 1, 2, 2, 3, 3, 0,
        //Top Loop
        4, 5, 5, 6, 6, 7, 7, 4,
        //Connecting Top and Bottom
        0, 4,
        1, 5,
        2, 6,
        3, 7
    };

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    Buffer cube_ibo(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
    Buffer cube_vbo;

    cube_vbo.set_data(vertices);
    cube_ibo.set_data(indices);

    cube_vbo.bind();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), NULL);
    glEnableVertexAttribArray(0);
    
    cube_ibo.bind();

    //glEnable(GL_CULL_FACE);
    //glCullFace(GL_BACK);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.f, 0.f, 0.f, 1.f);

    glm::dvec2 last_cursor_position = m_window->get_cursor_position() / m_window->get_window_size();

    float default_movement_speed = 5.0f;
    float mouse_sensitivity = 100.0f;

    while (!m_window->should_close()) {
        const double current_frame_time = glfwGetTime();
        const double frame_delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;

        m_window->poll_events();

        glm::dvec2 current_cursor_position = m_window->get_cursor_position();

        if (m_window->is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
            m_window->set_capture_mouse(true);

            const glm::dvec2 cursor_position_delta = (current_cursor_position - last_cursor_position) / m_window->get_window_size();
            glm::vec3 camera_rotation_delta = glm::vec3(cursor_position_delta.y, cursor_position_delta.x, 0.0f) * (float)frame_delta_time * mouse_sensitivity;

            camera.rotate(camera_rotation_delta);
        } else {
            m_window->set_capture_mouse(false);
        }

        
        glm::vec3 local_movement_delta(0.0f);
        float movement_speed = default_movement_speed;

        if (m_window->is_key_pressed(GLFW_KEY_LEFT_SHIFT)) {
            movement_speed *= 2.0f;
        }

        if (m_window->is_key_pressed(GLFW_KEY_W) || m_window->is_key_pressed(GLFW_KEY_UP)) {
            local_movement_delta.z += 1.0f;
        }
        if (m_window->is_key_pressed(GLFW_KEY_S) || m_window->is_key_pressed(GLFW_KEY_DOWN)) {
            local_movement_delta.z -= 1.0f;
        }

        if (m_window->is_key_pressed(GLFW_KEY_D) || m_window->is_key_pressed(GLFW_KEY_RIGHT)) {
            local_movement_delta.x += 1.0f;
        }
        if (m_window->is_key_pressed(GLFW_KEY_A) || m_window->is_key_pressed(GLFW_KEY_LEFT)) {
            local_movement_delta.x -= 1.0f;
        }

        if (m_window->is_key_pressed(GLFW_KEY_SPACE)) {
            local_movement_delta.y += 1.0f;
        }
        if (m_window->is_key_pressed(GLFW_KEY_LEFT_CONTROL)) {
            local_movement_delta.y -= 1.0f;
        }
        
        if (glm::abs(local_movement_delta.x) + glm::abs(local_movement_delta.y) + glm::abs(local_movement_delta.z) != 0.0f) {
            camera.move_local(local_movement_delta * (float)frame_delta_time * movement_speed);
        }

        if (camera.is_view_matrix_outdated()) {
            LOG_DEBUG("Camera view outdated");
            U_view.set(camera.view_matrix());
        }

        last_cursor_position = current_cursor_position;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPointSize(10.0f);
        glDrawElements(GL_LINES, indices.size(), GL_UNSIGNED_INT, 0);

        m_window->swapBuffers();
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
