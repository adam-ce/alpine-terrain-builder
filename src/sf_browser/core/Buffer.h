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