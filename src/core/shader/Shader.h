#pragma once
#include <glad/gl.h>
#include <string>

class Shader {
public:
	Shader(GLenum type);
	void compile(std::string_view code);
	GLuint handle();
	~Shader();

private:
	GLuint m_handle;
	GLenum m_type;

	bool handle_valid();
};