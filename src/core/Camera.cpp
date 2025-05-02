#include "Camera.h"
#include <utils/log/Log.h>

Camera::Camera(CameraConfig config) : 
	m_fov_deg(config.fov_deg), 
	m_aspect_ratio(config.aspect_ratio), 
	m_near_plane(config.near_plane), 
	m_far_plane(config.far_plane),
	m_position(config.position),
	m_up(glm::normalize(config.up)){
	
	glm::vec3 forward = glm::normalize(config.target - m_position);
	m_rotation = glm::quatLookAt(forward, m_up);

	update_projection_matrix();
	update_view_matrix();
}

void Camera::rotate(glm::vec3 euler_radians_delta) {
	m_view_matrix_cache.reset();

	glm::vec3 euler_rot = glm::eulerAngles(m_rotation);
	glm::vec3 new_euler_rot = euler_rot + euler_radians_delta;

	new_euler_rot.x = glm::mod(new_euler_rot.x, glm::two_pi<float>());
	new_euler_rot.y = glm::mod(new_euler_rot.y, glm::two_pi<float>());
	new_euler_rot.z = glm::mod(new_euler_rot.z, glm::two_pi<float>());

	LOG_DEBUG("{} / {} / {} => {} / {} / {}", euler_rot.x, euler_rot.y, euler_rot.z, new_euler_rot.x, new_euler_rot.y, new_euler_rot.z);

	m_rotation = glm::quat(new_euler_rot);
}

void Camera::move_local(glm::vec3 local_movement_delta) {
	m_view_matrix_cache.reset();

	glm::vec3 right_dir = local_movement_delta.x * glm::normalize(m_rotation * glm::vec3(1.0f, 0.0f, 0.0f));
	glm::vec3 up_dir = local_movement_delta.y * glm::normalize(m_rotation * m_up);
	glm::vec3 forward_dir = local_movement_delta.z * glm::normalize(m_rotation * glm::vec3(0.0f, 0.0f, -1.0f));

	auto copy = glm::vec3(m_position);

	m_position += right_dir + up_dir + forward_dir;
}

bool Camera::is_view_matrix_outdated() {
	return !m_view_matrix_cache.has_value();
}

bool Camera::is_projection_matrix_outdated() {
	return !m_projection_matrix_cache.has_value();
}

glm::mat4 Camera::projection_matrix() {
	update_projection_matrix();

	return m_projection_matrix_cache.value();
}

glm::mat4 Camera::view_matrix() {
	update_view_matrix();

	return m_view_matrix_cache.value();
}

void Camera::update_view_matrix() {
	if (!m_view_matrix_cache.has_value()) {
		glm::mat4 rotation = glm::mat4_cast(m_rotation);
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), m_position);
		m_view_matrix_cache.emplace(glm::inverse(translation * rotation));
	}
}

void Camera::update_projection_matrix() {
	if (!m_projection_matrix_cache.has_value()) {
		m_projection_matrix_cache.emplace(glm::perspective(glm::radians(m_fov_deg), m_aspect_ratio, m_near_plane, m_far_plane));
	}
}
