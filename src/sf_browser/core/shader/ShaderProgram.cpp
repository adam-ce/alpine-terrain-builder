#include "ShaderProgram.h"
#include <glad/gl.h>
#include <log.h>
#include <utility>

ShaderProgram::ShaderProgram() {
	m_handle = glCreateProgram();
}

void ShaderProgram::attach(Shader shader) {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried attaching Shader to ShaderProgram with invalid handle!");
	}
	glAttachShader(m_handle, shader.handle());
}

void ShaderProgram::link() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried linking ShaderProgram with invalid handle!");
	}

	glLinkProgram(m_handle);

	// check link result
	GLint succeded;
	glGetProgramiv(m_handle, GL_LINK_STATUS, &succeded);
	if (succeded == GL_FALSE) {
		// get log length
		GLint log_size;
		glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &log_size);

		// read log into buffer
		std::string message;
		message.resize(log_size);
		glGetProgramInfoLog(m_handle, log_size, nullptr, &message[0]);

		LOG_ERROR_AND_EXIT(message);
	}

	// load all uniform locations

	GLint uniformCount;
	glGetProgramiv(m_handle, GL_ACTIVE_UNIFORMS, &uniformCount);
	LOG_DEBUG("Active Uniforms: {}", uniformCount);

	GLsizei length; // name length
	GLint size;     // size of the variable
	GLenum type;    // type of the variable (float, vec3 or mat4, etc)
	std::string name;
	name.resize(16); // name buffer
	for (GLint i = 0; i < uniformCount; i++) {
		size_t name_length = 0;
		while (true) {
			if (!std::in_range<GLsizei>(name.size())) {
				LOG_ERROR_AND_EXIT("Uniform-name buffer is too large for GLsizei");
			}

			const GLsizei buffer_size = static_cast<GLsizei>(name.size());
			glGetActiveUniform(m_handle, static_cast<GLuint>(i), buffer_size, &length, &size, &type, name.data());

			if (length < 0 || length >= buffer_size) {
				LOG_ERROR_AND_EXIT("glGetActiveUniform returned invalid length {} for buffer size {}", length, buffer_size);
			}

			name_length = static_cast<size_t>(length);
			if (name_length < name.size() - 1) {
				break;
			} else {
				name.resize(name.length() * 2);
			}
		}

		const auto location = glGetUniformLocation(m_handle, name.c_str());
		m_uniform_locations.insert(std::make_pair(name.substr(0, name_length), location));
		LOG_DEBUG("Uniform #{} Name: {}", i, name.substr(0, name_length));
	}

	// load all attribute locations

	GLint attribute_count;
	glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTES, &attribute_count);
}

GLuint ShaderProgram::handle() {
	return m_handle;
}

GLint ShaderProgram::get_uniform_location(std::string name) {
	const auto result = m_uniform_locations.find(name);
	if (result == m_uniform_locations.end()) {
		LOG_ERROR_AND_EXIT("Could not find Uniform '{}'", name);
	} else {
		return result->second;
	}
}

void ShaderProgram::use() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried using ShaderProgram with invalid handle!");
	}
	glUseProgram(m_handle);
}

ShaderProgram::~ShaderProgram() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried destructing ShaderProgram with invalid handle!");
	}
	glDeleteProgram(m_handle);
}

bool ShaderProgram::handle_valid() {
	return m_handle != 0;
}
