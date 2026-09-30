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
#include <glad/gl.h>
#include <vector>

class Buffer {
public:
    Buffer() : Buffer(GL_ARRAY_BUFFER, GL_STATIC_DRAW) {}
	Buffer(GLenum target, GLenum usage);

	GLuint handle();
	void bind();

    template <typename T>
    inline void set_data(const std::vector<T>& data) {
        this->set_data(data.data(), data.size() * sizeof(T));
    }

    template <typename T, size_t N>
    inline void set_data(const std::array<T, N>& data) {
        this->set_data(data.data(), sizeof(std::array<T, N>));
    }

    void set_data(const void* data, const size_t size);

	~Buffer();

private:
	GLuint m_handle;
	GLenum m_target;
	GLenum m_usage;

	bool handle_valid();
};