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
#include <unordered_map>
#include "Shader.h"
#include "Uniform.h"

class ShaderProgram {
public:
	ShaderProgram();

	void attach(Shader shader);
	
	void link();
	
	GLuint handle();

	GLint get_uniform_location(std::string name);

	template <typename T>
	Uniform<T> get_uniform(std::string name) {
		return Uniform<T>(get_uniform_location(name));
	}
	
	void use();

	~ShaderProgram();
private:
	GLuint m_handle;
	std::unordered_map<std::string, GLint> m_uniform_locations;

	bool handle_valid();
};