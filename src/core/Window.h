#pragma once
#include <string>
#include <atomic>
#include <GLFW/glfw3.h>

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
private:
	static std::atomic<size_t> window_instances;
	static std::atomic<bool> glfw_initialized;

	int m_width, m_height;
	std::string m_title;

	GLFWwindow* m_window;

	static void glfw_error_callback(int error, const char* description);
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

	static void update_window_count(int delta);
};
