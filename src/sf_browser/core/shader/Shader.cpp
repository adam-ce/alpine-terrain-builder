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

#include "Shader.h"
#include <log.h>

Shader::Shader(GLenum type) : m_type(type) {
	m_handle = glCreateShader(m_type);
}

void Shader::compile(std::string_view code) {
    if (!handle_valid()) {
        LOG_ERROR_AND_EXIT("Tried compiling source for Shader with invalid handle!");
    }

    const char* code_ptr = code.data();
    const GLint code_length = code.length();
    glShaderSource(m_handle, 1, &code_ptr, &code_length);
    glCompileShader(m_handle);

    // check compilation result
    GLint succeded;
    glGetShaderiv(m_handle, GL_COMPILE_STATUS, &succeded);
    if (succeded == GL_FALSE) {
        // get log length
        GLint log_size;
        glGetShaderiv(m_handle, GL_INFO_LOG_LENGTH, &log_size);

        // read log into buffer
        std::string message;
        message.resize(log_size);
        glGetShaderInfoLog(m_handle, log_size, nullptr, &message[0]);

        LOG_ERROR_AND_EXIT(message);
    }
}

GLuint Shader::handle() {
    if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried getting handle of Shader with invalid handle!");
	}

    return m_handle;
}

Shader::~Shader() {
	if (handle_valid()) {
		glDeleteShader(m_handle);
	}
}

bool Shader::handle_valid() {
    return m_handle != 0;
}
