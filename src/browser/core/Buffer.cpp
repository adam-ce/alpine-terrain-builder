#include "Buffer.h"
#include <log.h>

Buffer::Buffer(GLenum target, GLenum usage) : m_target(target), m_usage(usage) {
	glCreateBuffers(1, &m_handle);
}

GLuint Buffer::handle() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried getting handle of Buffer with invalid handle!");
	}
	return GLuint();
}

void Buffer::bind() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried binding Buffer with invalid handle!");
	}

	glBindBuffer(m_target, m_handle);
}

void Buffer::set_data(const void* data, const size_t size) {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried setting data to Buffer with invalid handle!");
	}

	glNamedBufferData(m_handle, size, data, m_usage);
}

Buffer::~Buffer() {
	if (!handle_valid()) {
		LOG_ERROR_AND_EXIT("Tried destructing Buffer with invalid handle!");
	}
	glDeleteBuffers(1, &m_handle);
}

bool Buffer::handle_valid() {
	return m_handle != 0;
}
