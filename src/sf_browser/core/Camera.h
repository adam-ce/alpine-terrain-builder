/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Adrian Gawor
 * Copyright (C) 2025 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
	
	float get_aspect_ratio();
	void set_aspect_ratio(float new_aspect_ratio);

	float get_fov();
	void set_fov(float new_fov_deg);

	float get_near();
	float get_far();

	glm::dvec3 get_position();
	void set_position(glm::dvec3 new_position);

	glm::quat get_rotation_quat();
	void set_rotation_quat(glm::quat new_rotation_quat);

	glm::vec3 get_rotation_euler();
	void set_rotation_euler(glm::vec3 new_rotation_euler_radians);

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
