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
