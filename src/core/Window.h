#pragma once
#include <string>
#include <map>
#include <atomic>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct WindowConfig {
	int width = 1280;
	int height = 720;
	std::string title = "Window Title";
	bool resizeable = true;

	std::tuple<int, int> opengl_version = { 4, 6 };
	bool opengl_core_profile = true;
};

class Window {
public:
	Window(WindowConfig config);
	~Window();

	bool should_close();
	void poll_events();
	void swapBuffers();

	void set_capture_mouse(bool captured);

	glm::dvec2 get_cursor_position();
	glm::dvec2 get_window_size();
	bool is_key_pressed(int key);
	bool is_mouse_button_pressed(int key);

	float getAspectRatio();

private:
	static std::atomic<size_t> window_instances;
	static std::atomic<bool> glfw_initialized;

	int m_width, m_height;
	std::string m_title;

	GLFWwindow* m_window;

	std::map<int, bool> m_mouse_button_states;
	std::map<int, bool> m_key_states;
	glm::dvec2 m_current_cursor_pos;

	static void glfw_error_callback(int error, const char* description);
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

	static void update_window_count(int delta);
};
