#include "Window.h"
#include <utils/log/Log.h>

std::atomic<bool> Window::glfw_initialized(false);
std::atomic<size_t> Window::window_instances(0);

Window::Window(WindowConfig config) : m_width(config.width), m_height(config.height), m_title(config.title) {
    update_window_count(1);

    const auto [gl_major_version, gl_minor_version] = config.opengl_version;

    LOG_GL_INFO("Creating window with context: OpenGL {}.{}{}", gl_major_version, gl_minor_version, config.opengl_core_profile ? " CORE" : "");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major_version);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor_version);
    glfwWindowHint(GLFW_OPENGL_PROFILE, config.opengl_core_profile ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizeable ? GLFW_TRUE : GLFW_FALSE);

#ifdef _DEBUG
    // enable debug mode
    LOG_GL_DEBUG("Setting OpenGL Debug Context");
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_FALSE);
    // doesnt work: glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif // _DEBUG

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), NULL, NULL);
    if (m_window == NULL) {
        update_window_count(-1);
        LOG_GL_FATAL_AND_EXIT("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_window);

    // Set the required callback functions
    glfwSetKeyCallback(m_window, key_callback);
}

Window::~Window() {
    LOG_GL_INFO("Destroying Window \"{}\"", m_title);
    glfwDestroyWindow(m_window);

    update_window_count(-1);
}

bool Window::should_close() {
    return glfwWindowShouldClose(m_window);
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::glfw_error_callback(int error, const char* description) {
    LOG_GLFW_ERROR("[{}] {}", error, description);
}

void Window::key_callback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

void Window::update_window_count(int delta) {
    if (delta == 0) {
        return;
    }

    auto w_instances = Window::window_instances.load(std::memory_order_acquire);
    auto g_initialized = Window::glfw_initialized.load(std::memory_order_acquire);

    if (w_instances + delta < 0) {
        LOG_GLFW_FATAL_AND_EXIT("Illegal window count change from {} >> {} by {}!", w_instances, w_instances + delta, delta);
    }

    LOG_GLFW_INFO("Window count changed from {} >> {} by {}!", w_instances, w_instances + delta, delta);

    // If the previous window count was 0 AND glfw is not initialized, initialize GLFW
    bool needs_glfw_init = w_instances == 0 && !Window::glfw_initialized;

    // If the current window count is 0, AND glfw is initialized, destruct GLFW
    bool needs_glfw_destruction = w_instances + delta == 0 && Window::glfw_initialized;

    if (needs_glfw_init) {
        LOG_GLFW_INFO("Initializing GLFW");
        glfwInit();
        glfwSetErrorCallback(glfw_error_callback);

        g_initialized = true;
        Window::glfw_initialized.store(g_initialized, std::memory_order_release);
    }

    if (needs_glfw_destruction) {
        LOG_GLFW_INFO("Terminating GLFW");

        glfwTerminate();

        g_initialized = false;
        Window::glfw_initialized.store(g_initialized, std::memory_order_release);
    }

    w_instances += delta;
    Window::window_instances.store(w_instances, std::memory_order_release);
}
