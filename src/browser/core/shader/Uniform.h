#pragma once
#include <glad/gl.h>
#include "GLUniformAbstractions.h"

template <typename T>
class Uniform {
public:
	Uniform(GLint location) : m_location(location) {}
	void set(const T& value) {
		glUniform(m_location, value);
	}

private:
	GLint m_location;
};
