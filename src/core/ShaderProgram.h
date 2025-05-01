#pragma once
#include <unordered_map>
#include "Shader.h"
#include "Uniform.h"

class ShaderProgram {
public:
	ShaderProgram();

	void attach(Shader shader);
	
	void link();
	
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