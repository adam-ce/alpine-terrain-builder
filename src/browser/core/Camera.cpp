#include "Camera.h"
#include <log.h>

Camera::Camera(CameraConfig config) : 
	m_fov_deg(config.fov_deg), 
	m_aspect_ratio(config.aspect_ratio), 
	m_near_plane(config.near_plane), 
	m_far_plane(config.far_plane),
	m_up(glm::normalize(config.up)),
	m_position(config.position){
	
	glm::dvec3 forward = glm::normalize(config.target - m_position);
	m_rotation = glm::quatLookAt(forward, m_up);

	update_projection_matrix();
	update_view_matrix();
}

void Camera::rotate(double delta_yaw, double delta_pitch, double delta_roll) {
	m_view_matrix_cache.reset();

	glm::dquat pitch = glm::angleAxis(delta_pitch, get_local_right_dir());
	glm::dquat yaw = glm::angleAxis(delta_yaw, get_local_up_dir());
	glm::dquat roll = glm::angleAxis(delta_roll, get_local_forward_dir());

	glm::dquat rotation = yaw * pitch * roll;

	m_rotation = rotation * m_rotation;
}

void Camera::move_local(glm::dvec3 local_movement_delta) {
	m_view_matrix_cache.reset();

	glm::dvec3 right_dir = local_movement_delta.x * get_local_right_dir();
	glm::dvec3 up_dir = local_movement_delta.y * get_local_up_dir();
	glm::dvec3 forward_dir = local_movement_delta.z * get_local_forward_dir();

	m_position += right_dir + up_dir + forward_dir;
}

void Camera::set_near(float near) {
	LOG_TRACE("SETTING NEAR: {}", near);
	m_projection_matrix_cache.reset();

	m_near_plane = near;
}

void Camera::set_far(float far) {
	LOG_TRACE("SETTING FAR: {}", far);
	m_projection_matrix_cache.reset();

	m_far_plane = far;
}

float Camera::get_aspect_ratio() {
	return m_aspect_ratio;
}

void Camera::set_aspect_ratio(float new_aspect_ratio) {
	m_projection_matrix_cache.reset();

	m_aspect_ratio = new_aspect_ratio;
}

float Camera::get_fov() {
	return m_fov_deg;
}

void Camera::set_fov(float new_fov_deg) {
	m_projection_matrix_cache.reset();

	m_fov_deg = new_fov_deg;
}

float Camera::get_near() {
	return m_near_plane;
}

float Camera::get_far() {
	return m_far_plane;
}

glm::dvec3 Camera::get_position() {
	return m_position;
}

void Camera::set_position(glm::dvec3 new_position) {
	m_view_matrix_cache.reset();

	m_position = new_position;
}

glm::quat Camera::get_rotation_quat() {
	return m_rotation;
}

void Camera::set_rotation_quat(glm::quat new_rotation_quat) {
	m_view_matrix_cache.reset();

	m_rotation = new_rotation_quat;
}

glm::vec3 Camera::get_rotation_euler() {
	return glm::eulerAngles(m_rotation);
}

void Camera::set_rotation_euler(glm::vec3 new_rotation_euler_radians) {
	m_view_matrix_cache.reset();

	m_rotation = glm::quat(new_rotation_euler_radians);
}

glm::dvec3 Camera::get_local_right_dir() {
	return glm::normalize(m_rotation * glm::dvec3(1.0f, 0.0f, 0.0f));
}

glm::dvec3 Camera::get_local_up_dir() {
	return glm::normalize(m_rotation * m_up);
}

glm::dvec3 Camera::get_local_forward_dir() {
	return glm::normalize(m_rotation * glm::dvec3(0.0f, 0.0f, -1.0f));
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
		//glm::mat4 translation = glm::translate(glm::dmat4(1.0f), m_position); //TODO: Use camera re-centering to mitigate precision losses at large values
		m_view_matrix_cache.emplace(glm::inverse(/*translation */ rotation));
	}
}

void Camera::update_projection_matrix() {
	if (!m_projection_matrix_cache.has_value()) {
		m_projection_matrix_cache.emplace(glm::perspective(glm::radians(m_fov_deg), m_aspect_ratio, m_near_plane, m_far_plane));
	}
}
