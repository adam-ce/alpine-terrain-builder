#pragma once

#include <cstdint>
#include <vector>

#include <libassert/assert.hpp>

template <typename T>
struct OffsetVector {
    std::vector<T> data;
    size_t offset = 0;

    OffsetVector() = default;
    OffsetVector(std::vector<T> data, size_t offset = 0)
        : data(std::move(data)), offset(offset) {}

    [[nodiscard]] size_t size() const {
        return this->data.size();
    }
    [[nodiscard]] bool empty() const {
        return this->data.empty();
    }

    void resize(size_t new_size, const T& default_value={}) {
        this->data.resize(new_size, default_value);
    }

    [[nodiscard]] bool contains(size_t index) const {
        return index >= this->offset && index < this->offset + this->size();
    }

    [[nodiscard]] T &operator[](size_t index) {
        DEBUG_ASSERT(this->contains(index));
        return this->data[index - this->offset];
    }

    [[nodiscard]] const T &operator[](size_t index) const {
        DEBUG_ASSERT(this->contains(index));
        return this->data[index - this->offset];
    }

    [[nodiscard]] auto begin() {
        return this->data.begin();
    }
    [[nodiscard]] auto end() {
        return this->data.end();
    }
    [[nodiscard]] auto begin() const {
        return this->data.begin();
    }
    [[nodiscard]] auto end() const {
        return this->data.end();
    }
};
