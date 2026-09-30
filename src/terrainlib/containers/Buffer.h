/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#pragma once

#include <array>
#include <memory>
#include <span>

#include <libassert/assert.hpp>

#include "containers/Size.h"

template <typename T, Size S>
class BufferStorage;

template <typename T, size_t N>
class BufferStorage<T, ComptimeSize<N>> {
public:
    constexpr size_t size() const noexcept {
        return N;
    }

    constexpr T *data() noexcept {
        return this->storage.data();
    }
    constexpr const T *data() const noexcept {
        return this->storage.data();
    }

    constexpr std::span<T, N> span() noexcept {
        return std::span<T, N>(this->storage);
    }
    constexpr std::span<const T, N> span() const noexcept {
        return std::span<const T, N>(this->storage);
    }

private:
    std::array<T, N> storage{};
};

template <typename T>
class BufferStorage<T, RuntimeSize> {
public:
    explicit BufferStorage(const size_t n)
        : storage(std::make_unique<T[]>(n)), n(n) {}

    size_t size() const noexcept {
        return this->n;
    }

    T *data() noexcept {
        return this->storage.get();
    }
    const T *data() const noexcept {
        return this->storage.get();
    }

    std::span<T> span() noexcept {
        return std::span<T>(this->data(), this->size());
    }
    std::span<const T> span() const noexcept {
        return std::span<const T>(this->data(), this->size());
    }

private:
    std::unique_ptr<T[]> storage;
    size_t n;
};

template <typename T, Size S>
class Buffer {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;

    constexpr Buffer()
        requires(S::is_comptime())
    = default;

    explicit Buffer(const size_t n)
        requires(S::is_runtime())
        : storage(n) {}

    constexpr size_t size() const noexcept {
        return this->storage.size();
    }
    constexpr bool empty() const noexcept {
        return this->size() == 0;
    }

    constexpr pointer data() noexcept {
        return this->storage.data();
    }
    constexpr const_pointer data() const noexcept {
        return this->storage.data();
    }

    constexpr auto span() noexcept {
        return this->storage.span();
    }
    constexpr auto span() const noexcept {
        return this->storage.span();
    }

    constexpr reference operator[](const size_t i) noexcept {
        DEBUG_ASSERT(i < this->size());
        return this->data()[i];
    }
    constexpr const_reference operator[](const size_t i) const noexcept {
        DEBUG_ASSERT(i < this->size());
        return this->data()[i];
    }

    reference at(const size_t i) {
        if (i >= this->size()) {
            throw std::out_of_range("Buffer::at");
        }
        return this->data()[i];
    }
    const_reference at(const size_t i) const {
        if (i >= this->size()) {
            throw std::out_of_range("Buffer::at");
        }
        return this->data()[i];
    }

    reference front() {
        return this->at(0);
    }
    const_reference front() const {
        return this->at(0);
    }
    reference back() {
        return this->at(this->size() - 1);
    }
    const_reference back() const {
        return this->at(this->size() - 1);
    }

    constexpr iterator begin() noexcept {
        return this->data();
    }
    constexpr const_iterator begin() const noexcept {
        return this->data();
    }
    constexpr const_iterator cbegin() const noexcept {
        return this->data();
    }

    constexpr iterator end() noexcept {
        return this->data() + this->size();
    }
    constexpr const_iterator end() const noexcept {
        return this->data() + this->size();
    }
    constexpr const_iterator cend() const noexcept {
        return this->data() + this->size();
    }

private:
    BufferStorage<T, S> storage;
};

inline constexpr size_t MAX_STACK_BYTES = 1024;

template <typename T>
inline constexpr Buffer<T, RuntimeSize> make_heap_buffer(const size_t size) {
    return Buffer<T, RuntimeSize>(size);
}
template <typename T, size_t N>
inline constexpr Buffer<T, ComptimeSize<N>> make_stack_buffer() {
    return Buffer<T, ComptimeSize<N>>();
}

template <class T, Size S>
auto make_buffer(const S s) {
    if constexpr (S::is_comptime()) {
        constexpr size_t N = S::value();
        if constexpr (sizeof(T) * N < MAX_STACK_BYTES) {
            return make_stack_buffer<T, N>();
        } else {
            return make_heap_buffer<T>(N);
        }
    } else {
        return make_heap_buffer<T>(s);
    }
}