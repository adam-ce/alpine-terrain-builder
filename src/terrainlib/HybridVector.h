#pragma once

#include "FixedVector.h"
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
template <typename T, std::size_t N>
class HybridVector {
public:
    HybridVector() : _is_on_stack(true) {}

    template <typename U>
    void push_back(U &&value) {
        this->emplace_back(std::forward<U>(value));
    }

    template <typename... Args>
    void emplace_back(Args &&...args) {
        if (this->_is_on_stack) {
            if (this->_stack.full()) {
                this->switch_to_heap();
                this->_heap.emplace_back(std::forward<Args>(args)...);
            } else {
                this->_stack.emplace_back(std::forward<Args>(args)...);
            }
        } else {
            this->_heap.emplace_back(std::forward<Args>(args)...);
        }
    }

    void pop_back() {
        if (this->_is_on_stack) {
            this->_stack.pop_back();
        } else {
            this->_heap.pop_back();
        }
    }

    void reserve(size_t new_capacity) {
        if (this->_is_on_stack) {
            if (new_capacity > N) {
                this->switch_to_heap(new_capacity);
            }
        } else {
            this->_heap.reserve(new_capacity);
        }
    }

    void resize(size_t new_size) {
        if (this->_is_on_stack) {
            if (new_size > N) {
                this->switch_to_heap(new_size);
                this->_heap.resize(new_size);
            } else {
                this->_stack.resize(new_size);
            }
        } else {
            this->_heap.resize(new_size);
        }
    }

    T *data() {
        return this->_is_on_stack ? &this->_stack.data() : this->_heap.data();
    }
    const T *data() const {
        return this->_is_on_stack ? &this->_stack.data() : this->_heap.data();
    }

    size_t size() const {
        return this->_is_on_stack ? this->_stack.size() : this->_heap.size();
    }
    bool empty() const {
        return this->size() == 0;
    }

    T &operator[](size_t i) {
        return this->_is_on_stack ? this->_stack[i] : this->_heap[i];
    }
    const T &operator[](size_t i) const {
        return this->_is_on_stack ? this->_stack[i] : this->_heap[i];
    }

    T &at(size_t index) {
        return this->_is_on_stack ? this->_stack.at(index) : this->_heap.at(index);
    }
    const T &at(size_t index) const {
        return this->_is_on_stack ? this->_stack.at(index) : this->_heap.at(index);
    }

    T* begin() {
        return this->_is_on_stack ? this->_stack.begin() : this->_heap.begin();
    }
    T* end() {
        return this->_is_on_stack ? this->_stack.end() : this->_heap.end();
    }
    const T* begin() const {
        return this->_is_on_stack ? this->_stack.data() : this->_heap.data();
    }
    const T* end() const {
        return this->_is_on_stack ? this->_stack.data() + this->_stack.size() : this->_heap.data() + this->_heap.size();
    }
    const T* cbegin() const {
        return this->begin();
    }
    const T* cend() const {
        return this->end();
    }

    operator std::span<T>() {
        return std::span<T>(this->data(), this->size());
    }
    operator std::span<const T>() const {
        return std::span<const T>(this->data(), this->size());
    }

private:
    void switch_to_heap(const size_t reserve_size = N * 2) {
        this->_heap.reserve(reserve_size);
        for (auto &v : this->_stack) {
            this->_heap.push_back(std::move(v));
        }
        this->_is_on_stack = false;
    }

    FixedVector<T, N> _stack;
    std::vector<T> _heap;
    bool _is_on_stack;
};
}
