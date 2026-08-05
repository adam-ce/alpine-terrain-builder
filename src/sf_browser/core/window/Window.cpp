#include "Window.h"
#include <log.h>
#include <limits>

std::atomic<bool> Window::glfw_initialized(false);
std::atomic<size_t> Window::window_instances(0);

Window::Window(WindowConfig config) : m_width(config.width), m_height(config.height), m_title(config.title) /*, m_msaa_samples(config.msaa_samples)*/ {
    update_window_count(1);

    const auto [gl_major_version, gl_minor_version] = config.opengl_version;

    LOG_INFO("Creating window \"{}\" with context: OpenGL {}.{}{}", m_title, gl_major_version, gl_minor_version, config.opengl_core_profile ? " CORE" : "");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, gl_major_version);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, gl_minor_version);
    glfwWindowHint(GLFW_OPENGL_PROFILE, config.opengl_core_profile ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizeable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_SAMPLES, config.msaa_samples);

#ifdef _DEBUG
    // enable debug mode
    LOG_DEBUG("Setting OpenGL Debug Context");
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_NO_ERROR, GLFW_FALSE);
#endif // _DEBUG

    m_handle = glfwCreateWindow(m_width, m_height, m_title.c_str(), NULL, NULL);
    if (m_handle == NULL) {
        update_window_count(-1);
        LOG_ERROR_AND_EXIT("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(m_handle);

    // Set the required callback functions
    glfwSetKeyCallback(m_handle, key_callback);
    glfwSetMouseButtonCallback(m_handle, mouse_button_callback);
    glfwSetCursorPosCallback(m_handle, cursor_position_callback);
    glfwSetScrollCallback(m_handle, scroll_callback);
    glfwSetFramebufferSizeCallback(m_handle, framebuffer_size_callback);

    // Initialize current and last cursor positions
    glfwGetCursorPos(m_handle, &m_current_unfetched_cursor_pos.x, &m_current_unfetched_cursor_pos.y);
    clear_accumulated_cursor_delta();

    m_min_dimension = glm::min(m_width, m_height);
    m_max_dimension = glm::max(m_width, m_height);

    // Link a pointer to this class with the window handle, to be able to access this class from the callbacks
    glfwSetWindowUserPointer(m_handle, (void*)this);
}

Window::~Window() {
    LOG_INFO("Destroying Window \"{}\"", m_title);
    glfwDestroyWindow(m_handle);

    update_window_count(-1);
}

bool Window::should_close() {
    return glfwWindowShouldClose(m_handle);
}

void Window::set_should_close(bool should_close) {
    glfwSetWindowShouldClose(m_handle, should_close ? GL_TRUE : GL_FALSE);
}

void Window::set_title_suffix(std::string suffix) {
    glfwSetWindowTitle(m_handle, (m_title + suffix).c_str());
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_handle);
}

bool Window::is_mouse_captured() {
    return glfwGetInputMode(m_handle, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
}

void Window::toggle_capture_mouse() {
    if (is_mouse_captured()) {
        set_capture_mouse(false);
    } else {
        set_capture_mouse(true);
    }
}

void Window::set_capture_mouse(bool captured) {
    if (captured) {
        glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetInputMode(m_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    } else {
        glfwSetInputMode(m_handle, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

glm::dvec2 Window::get_accumulated_cursor_delta() {
    glm::dvec2 delta = (m_current_unfetched_cursor_pos - m_last_fetched_cursor_pos) / (double)m_max_dimension;

    clear_accumulated_cursor_delta();

    return delta;
}

void Window::clear_accumulated_cursor_delta() {
    m_last_fetched_cursor_pos = m_current_unfetched_cursor_pos;;
}

glm::dvec2 Window::get_cursor_position() {
    glm::dvec2 cursor_position;
    glfwGetCursorPos(m_handle, &cursor_position.x, &cursor_position.y);
    return cursor_position;
}

glm::dvec2 Window::get_window_size() {
    return glm::dvec2(m_width, m_height);
}

GLFWwindow* Window::handle() {
    return m_handle;
}

bool Window::is_key_pressed(int key) {
    auto result = m_key_states.find(key);
    
    if (result == m_key_states.end()) {
        return false;
    }

    return result->second;
}

bool Window::is_mouse_button_pressed(int button) {
    auto result = m_mouse_button_states.find(button);

    if (result == m_mouse_button_states.end()) {
        return false;
    }

    return result->second;
}

void Window::register_key_event(int action, int key, std::function<void()> callback) {
    m_key_callbacks.try_emplace({ action, key }, std::vector<std::function<void()>>(0));

    auto callbacks = m_key_callbacks.find({ action, key });

    if (callbacks != m_key_callbacks.end()) {
        callbacks->second.push_back(callback);
    }
}

void Window::register_scroll_event(std::function<void(glm::dvec2)> callback) {
    m_scroll_callbacks.push_back(callback);
}

void Window::register_framebuffer_resize_event(std::function<void(glm::ivec2)> callback) {
    m_framebuffer_resize_callbacks.push_back(callback);
}

float Window::getAspectRatio() {
    return (float)m_width / (float)m_height;
}

void Window::glfw_error_callback(int error, const char* description) {
    LOG_ERROR("[{}] {}", error, description);
}

void Window::key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mode*/) {
    Window* w = (Window*)glfwGetWindowUserPointer(window);

    if (!w->m_key_states.contains(key)) {
        w->m_key_states.emplace(key, false);
    }

    if (action == GLFW_PRESS) {
        w->m_key_states[key] = true;
    } else if (action == GLFW_RELEASE) {
        w->m_key_states[key] = false;
    }

    w->m_key_callbacks.try_emplace({action, key}, std::vector<std::function<void()>>(0));

    auto callbacks = w->m_key_callbacks.find({ action, key });

    if (callbacks != w->m_key_callbacks.end()) {
        for (auto callback : callbacks->second) {
            callback();
        }
    }
}

void Window::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    Window* w = (Window*)glfwGetWindowUserPointer(window);
    
    if (!w->m_mouse_button_states.contains(button)) {
        w->m_mouse_button_states.emplace(button, false);
    }

    if (action == GLFW_PRESS) {
        w->m_mouse_button_states[button] = true;
    } else if (action == GLFW_RELEASE) {
        w->m_mouse_button_states[button] = false;
    }
}

void Window::cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    Window* w = (Window*)glfwGetWindowUserPointer(window);

    w->m_current_unfetched_cursor_pos.x = xpos;
    w->m_current_unfetched_cursor_pos.y = ypos;
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* w = (Window*)glfwGetWindowUserPointer(window);

    for (auto callback : w->m_scroll_callbacks) {
        callback(glm::dvec2(xoffset, yoffset));
    }
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    Window* w = (Window*)glfwGetWindowUserPointer(window);

    w->m_width = width;
    w->m_height = height;

    for (auto callback : w->m_framebuffer_resize_callbacks) {
        callback(glm::dvec2(width, height));
    }
}

void Window::update_window_count(int delta) {
    auto w_instances = Window::window_instances.load(std::memory_order_acquire);
    auto g_initialized = Window::glfw_initialized.load(std::memory_order_acquire);

    if (delta != 1 && delta != -1) {
        LOG_ERROR_AND_EXIT("Illegal window-count delta: {}", delta);
    }

    if (delta == -1 && w_instances == 0) {
        LOG_ERROR_AND_EXIT("Cannot decrement window count below zero");
    }

    if (delta == 1 && w_instances == std::numeric_limits<size_t>::max()) {
        LOG_ERROR_AND_EXIT("Cannot increment window count beyond size_t maximum");
    }

    const size_t new_count = delta == 1 ? w_instances + 1 : w_instances - 1;

    LOG_INFO("Window count changed from {} >> {} by {}!", w_instances, new_count, delta);

    // If the previous window count was 0 AND glfw is not initialized, initialize GLFW
    bool needs_glfw_init = w_instances == 0 && !Window::glfw_initialized;

    // If the current window count is 0, AND glfw is initialized, destruct GLFW
    bool needs_glfw_destruction = new_count == 0 && Window::glfw_initialized;

    if (needs_glfw_init) {
        LOG_INFO("Initializing GLFW");
        glfwInit();
        glfwSetErrorCallback(glfw_error_callback);

        g_initialized = true;
        Window::glfw_initialized.store(g_initialized, std::memory_order_release);
    }

    if (needs_glfw_destruction) {
        LOG_INFO("Terminating GLFW");

        glfwTerminate();

        g_initialized = false;
        Window::glfw_initialized.store(g_initialized, std::memory_order_release);
    }

    Window::window_instances.store(new_count, std::memory_order_release);
}
