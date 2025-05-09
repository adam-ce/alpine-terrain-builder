#pragma once
#include <string>
#include <unordered_map>
#include <map>
#include <atomic>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct WindowConfig {
	int width = 1280;
	int height = 720;
	std::string title = "Window Title";
	bool resizeable = true;
	int msaa_samples = 4;

	std::tuple<int, int> opengl_version = { 4, 6 };
	bool opengl_core_profile = true;
};

class Window {
public:
	Window(WindowConfig config);
	~Window();

	bool should_close();
	void set_should_close(bool should_close);

	void set_title_suffix(std::string suffix);

	void poll_events();
	void swapBuffers();

	bool is_mouse_captured();
	void toggle_capture_mouse();
	void set_capture_mouse(bool captured);
	
	glm::dvec2 get_accumulated_cursor_delta();
	void clear_accumulated_cursor_delta();

	glm::dvec2 get_cursor_position();
	glm::dvec2 get_window_size();
	bool is_key_pressed(int key);
	bool is_mouse_button_pressed(int key);

	void register_key_event(int action, int key, std::function<void()> callback);
	void register_scroll_event(std::function<void(glm::dvec2)> callback);
	void register_framebuffer_resize_event(std::function<void(glm::ivec2)> callback);

	float getAspectRatio();

private:
	static std::atomic<size_t> window_instances;
	static std::atomic<bool> glfw_initialized;

	int m_width, m_height, m_max_dimension, m_min_dimension;
	std::string m_title;
	int m_msaa_samples;

	GLFWwindow* m_handle;

	std::map<int, bool> m_mouse_button_states;
	std::map<int, bool> m_key_states;

	glm::dvec2 m_last_fetched_cursor_pos;
	glm::dvec2 m_current_unfetched_cursor_pos;

	std::map<std::tuple<int, int>, std::vector<std::function<void()>>> m_key_callbacks;
	std::vector<std::function<void(glm::dvec2)>> m_scroll_callbacks;
	std::vector<std::function<void(glm::ivec2)>> m_framebuffer_resize_callbacks;

	static void glfw_error_callback(int error, const char* description);
	static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
	static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
	static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
	static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

	static void update_window_count(int delta);
};
