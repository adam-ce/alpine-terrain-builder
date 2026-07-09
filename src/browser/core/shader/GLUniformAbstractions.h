#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace {
    // vectors
    template <typename T, size_t length>
    void glUniformv(const GLint location, const GLsizei count, const T* data);

    template <>
    inline void glUniformv<float, 1>(const GLint location, const GLsizei count, const float* data) {
        glUniform1fv(location, count, data);
    }
    template <>
    inline void glUniformv<float, 2>(const GLint location, const GLsizei count, const float* data) {
        glUniform2fv(location, count, data);
    }
    template <>
    inline void glUniformv<float, 3>(const GLint location, const GLsizei count, const float* data) {
        glUniform3fv(location, count, data);
    }
    template <>
    inline void glUniformv<float, 4>(const GLint location, const GLsizei count, const float* data) {
        glUniform4fv(location, count, data);
    }

    template <>
    inline void glUniformv<int, 1>(const GLint location, const GLsizei count, const int* data) {
        glUniform1iv(location, count, data);
    }
    template <>
    inline void glUniformv<int, 2>(const GLint location, const GLsizei count, const int* data) {
        glUniform2iv(location, count, data);
    }
    template <>
    inline void glUniformv<int, 3>(const GLint location, const GLsizei count, const int* data) {
        glUniform3iv(location, count, data);
    }
    template <>
    inline void glUniformv<int, 4>(const GLint location, const GLsizei count, const int* data) {
        glUniform4iv(location, count, data);
    }

    template <>
    inline void glUniformv<unsigned int, 1>(const GLint location, const GLsizei count, const unsigned int* data) {
        glUniform1uiv(location, count, data);
    }
    template <>
    inline void glUniformv<unsigned int, 2>(const GLint location, const GLsizei count, const unsigned int* data) {
        glUniform2uiv(location, count, data);
    }
    template <>
    void glUniformv<unsigned int, 3>(const GLint location, const GLsizei count, const unsigned int* data) {
        glUniform3uiv(location, count, data);
    }
    template <>
    void glUniformv<unsigned int, 4>(const GLint location, const GLsizei count, const unsigned int* data) {
        glUniform4uiv(location, count, data);
    }

    // matrices
    template <typename T, size_t width, size_t height>
    void glUniformMatrix(const GLint location, const GLsizei count, const bool transpose, const T* data);

    template <>
    inline void glUniformMatrix<float, 2, 2>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix2fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 3, 3>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix3fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 4, 4>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix4fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 2, 3>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix2x3fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 2, 4>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix2x4fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 4, 2>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix4x2fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 3, 4>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix3x4fv(location, count, transpose, data);
    }
    template <>
    inline void glUniformMatrix<float, 4, 3>(const GLint location, const GLsizei count, const bool transpose, const float* data) {
        glUniformMatrix4x3fv(location, count, transpose, data);
    }
} // namespace

template <typename T>
void glUniform(const GLint location, const T* data, const GLsizei count);

template <>
inline void glUniform(const GLint location, const float* data, const GLsizei count) {
    glUniform1fv(location, count, data);
}
template <>
inline void glUniform(const GLint location, const int* data, const GLsizei count) {
    glUniform1iv(location, count, data);
}
template <>
inline void glUniform(const GLint location, const unsigned int* data, const GLsizei count) {
    glUniform1uiv(location, count, data);
}
template <glm::length_t L, typename T, glm::precision Q>
void glUniform(const GLint location, const glm::vec<L, T, Q>* data, const GLsizei count) {
    glUniformv<T, L>(location, count, glm::value_ptr(*data));
}
template <glm::length_t W, glm::length_t H, typename T, glm::precision Q>
void glUniform(const GLint location, const glm::mat<W, H, T, Q>* data, const GLsizei count) {
    glUniformMatrix<T, W, H>(location, count, false, glm::value_ptr(*data));
}

template <typename T>
void glUniform(const GLint location, T data);

template <>
inline void glUniform(const GLint location, bool data) {
    glUniform1i(location, data ? GL_TRUE : GL_FALSE);
}
template <>
inline void glUniform(const GLint location, float data) {
    glUniform1f(location, data);
}
template <>
inline void glUniform(const GLint location, int data) {
    glUniform1i(location, data);
}
template <>
inline void glUniform(const GLint location, unsigned int data) {
    glUniform1ui(location, data);
}
template <glm::length_t L, typename T, glm::precision Q>
void glUniform(const GLint location, const glm::vec<L, T, Q>& data) {
    glUniformv<T, L>(location, 1, glm::value_ptr(data));
}
template <glm::length_t W, glm::length_t H, typename T, glm::precision Q>
void glUniform(const GLint location, const glm::mat<W, H, T, Q>& data) {
    glUniformMatrix<T, W, H>(location, 1, false, glm::value_ptr(data));
}
template <typename T, size_t length>
void glUniform(const GLint location, const std::array<T, length>& data) {
    glUniform<>(location, data.data(), (GLsizei)data.size());
}
template <typename T>
void glUniform(const GLint location, const std::vector<T>& data) {
    glUniform<T>(location, data.data(), (GLsizei)data.size());
}
