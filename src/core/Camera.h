#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>

struct CameraConfig {
	float fov_deg = 60.0f;
	float aspect_ratio = 16.0f / 9.0f;
	float near_plane = 0.1f;
	float far_plane = 100.0f;

	glm::dvec3 position = glm::dvec3(0.0f, 0.0f, 5.0f);
	glm::dvec3 target = glm::dvec3(0.0f);
	glm::dvec3 up = glm::dvec3(0.0f, 1.0f, 0.0f);
};

class Camera {
public:
	Camera(CameraConfig config);

	void rotate(double delta_yaw, double delta_pitch, double delta_roll);
	void move_local(glm::dvec3 local_movement_delta);
	void set_near(float near);
	void set_far(float far);

	float get_near();
	float get_far();

	glm::dvec3 get_position();

	glm::dvec3 get_local_right_dir();
	glm::dvec3 get_local_up_dir();
	glm::dvec3 get_local_forward_dir();

	bool is_view_matrix_outdated();
	bool is_projection_matrix_outdated();

	glm::mat4 projection_matrix();
	glm::mat4 view_matrix();

private:
	float m_fov_deg;
	float m_aspect_ratio;
	float m_near_plane;
	float m_far_plane;

	glm::dvec3 m_up;
	glm::dvec3 m_position;
	glm::dquat m_rotation;

	std::optional<glm::mat4> m_view_matrix_cache;
	std::optional<glm::mat4> m_projection_matrix_cache;

	void update_view_matrix();
	void update_projection_matrix();
};
