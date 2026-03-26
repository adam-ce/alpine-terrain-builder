#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <libassert/assert.hpp>

template <typename T, std::size_t N>
class FixedVector {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    using reference = value_type &;
    using const_reference = const value_type &;

    using pointer = value_type *;
    using const_pointer = const value_type *;

    using iterator = value_type *;
    using const_iterator = const value_type *;

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    
    constexpr FixedVector() = default;

    FixedVector(std::initializer_list<T> init) {
        if (init.size() > N) {
            throw std::out_of_range("capacity exceeded");
        }

        const size_t old_size = this->_size;
        try {
            for (const T &value : init) {
                this->construct_back(value);
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }
    }

    FixedVector(const FixedVector &other) {
        const size_t old_size = this->_size;
        try {
            for (size_t i = 0; i < other._size; ++i) {
                this->construct_back(other[i]);
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }
    }

    FixedVector &operator=(const FixedVector &other) {
        if (this == &other) {
            return *this;
        }

        const size_t common = std::min(this->_size, other._size);

        for (size_t i = 0; i < common; ++i) {
            (*this)[i] = other[i];
        }

        if (this->_size > other._size) {
            this->destroy_from(other._size);
            return *this;
        }

        const size_t old_size = this->_size;
        try {
            for (size_t i = common; i < other._size; ++i) {
                this->construct_back(other[i]);
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }

        return *this;
    }

    FixedVector(FixedVector &&other) {
        const size_t old_size = this->_size;
        try {
            for (size_t i = 0; i < other._size; ++i) {
                this->construct_back(std::move(other[i]));
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }
        other.clear();
    }

    FixedVector &operator=(FixedVector &&other) {
        if (this == &other) {
            return *this;
        }

        const size_t common = std::min(this->_size, other._size);

        for (size_t i = 0; i < common; ++i) {
            (*this)[i] = std::move(other[i]);
        }

        if (this->_size > other._size) {
            this->destroy_from(other._size);
            other.clear();
            return *this;
        }

        const size_t old_size = this->_size;
        try {
            for (size_t i = common; i < other._size; ++i) {
                this->construct_back(std::move(other[i]));
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }

        other.clear();
        return *this;
    }

    template <typename... Args>
    void emplace_back(Args &&...args) {
        const bool inserted = this->try_emplace_back(std::forward<Args>(args)...);
        if (!inserted) {
            throw std::out_of_range("capacity exceeded");
        }
    }

    template <typename... Args>
    bool try_emplace_back(Args &&...args) {
        if (this->_size >= N) {
            return false;
        }

        this->construct_at(this->_size, std::forward<Args>(args)...);
        this->_size++;
        return true;
    }

    template <class U>
    void push_back(U &&value) {
        static_assert(std::is_convertible_v<U, T>,
                      "push_back argument must be convertible to T");
        return this->emplace_back(std::forward<U>(value));
    }

    template <class U>
    bool try_push_back(U &&value) {
        static_assert(std::is_convertible_v<U, T>,
                      "push_back argument must be convertible to T");
        return this->try_emplace_back(std::forward<U>(value));
    }

    void pop_back() {
        if (this->_size == 0) {
            throw std::out_of_range("empty vector");
        }
        this->destroy_back();
    }
    
    T &back() {
        if (this->_size == 0) {
            throw std::out_of_range("empty vector");
        }
        return (*this)[this->_size - 1];
    }

    const T &back() const {
        if (this->_size == 0) {
            throw std::out_of_range("empty vector");
        }
        return (*this)[this->_size - 1];
    }

    T &operator[](const size_t i) {
        return this->get_at(i);
    }
    const T &operator[](const size_t i) const {
        return this->get_at(i);
    }

    T &at(const size_t i) {
        if (i >= this->_size) {
            throw std::out_of_range("index out of range");
        }
        return this->get_at(i);
    }
    const T &at(const size_t i) const {
        if (i >= this->_size) {
            throw std::out_of_range("index out of range");
        }
        return this->get_at(i);
    }

    T *data() {
        return std::launder(reinterpret_cast<T *>(this->_data));
    }
    const T *data() const {
        return std::launder(reinterpret_cast<const T *>(this->_data));
    }

    void clear() {
        this->destroy_from(0);
    }

    void resize(const size_t new_size) {
        if (new_size > N) {
            throw std::out_of_range("capacity exceeded");
        }

        if (new_size < this->_size) {
            this->destroy_from(new_size);
            return;
        }

        const size_t old_size = this->_size;
        try {
            while (this->_size < new_size) {
                this->construct_back();
            }
        } catch (...) {
            this->destroy_from(old_size);
            throw;
        }
    }

    constexpr size_t size() const {
        return this->_size;
    }
    constexpr size_t capacity() const {
        return N;
    }
    constexpr bool empty() const {
        return this->_size == 0;
    }
    constexpr bool full() const {
        return this->_size == N;
    }

    void erase(const size_t index) {
        if (index >= this->_size) {
            throw std::out_of_range("index out of range");
        }

        for (size_t i = index; i + 1 < this->_size; i++) {
            (*this)[i] = std::move((*this)[i + 1]);
        }
        this->pop_back();
    }
    T* erase(const T* it) {
        const T* first = this->begin();
        const T* last = this->end();

        if (it < first || it >= last) {
            throw std::out_of_range("iterator out of range");
        }

        const size_t index = static_cast<size_t>(it - first);
        this->erase(index);
        return this->data() + index;
    }
    T *erase(const T *first, const T *last) {
        T* begin = this->begin();
        T* end =  this->end();

        if (first < begin || first > end || last < first || last > end) {
            throw std::out_of_range("invalid range");
        }

        const size_t first_index = static_cast<size_t>(first - begin);
        const size_t last_index = static_cast<size_t>(last - end);

        const size_t count = last_index - first_index;
        if (count == 0) {
            return begin + first_index;
        }

        for (size_t i = first_index; i + count < this->size(); i++) {
            (*this)[i] = std::move((*this)[i + count]);
        }

        this->resize(this->size() - count);
        return begin + first_index;
    }

    T *begin() {
        return this->data();
    }
    T *end() {
        return this->data() + this->_size;
    }
    const T *begin() const {
        return this->data();
    }
    const T *end() const {
        return this->data() + this->_size;
    }

    operator std::span<T>() {
        return std::span<T>(this->data(), this->size());
    }

    operator std::span<const T>() const {
        return std::span<const T>(this->data(), this->size());
    }

    ~FixedVector() {
        this->clear();
    }

private:
    T &get_at(const size_t index) {
        DEBUG_ASSERT(index < this->_size);
        return *std::launder(reinterpret_cast<T *>(this->_data + index * sizeof(T)));
    }
    const T &get_at(const size_t index) const {
        DEBUG_ASSERT(index < this->_size);
        return *std::launder(reinterpret_cast<const T *>(this->_data + index * sizeof(T)));
    }

    template <typename... Args>
    void construct_at(const size_t index, Args &&...args) {
        DEBUG_ASSERT(index == this->_size && index < this->capacity());
        std::construct_at(reinterpret_cast<T *>(this->_data + index * sizeof(T)),
                          std::forward<Args>(args)...);
    }

    void destroy_at(const size_t index) {
        DEBUG_ASSERT(index < this->_size);
        std::destroy_at(std::launder(reinterpret_cast<T *>(this->_data + index * sizeof(T))));
    }

    template <typename... Args>
    void construct_back(Args &&...args) {
        DEBUG_ASSERT(this->_size < N);
        this->construct_at(this->_size, std::forward<Args>(args)...);
        this->_size++;
    }

    void destroy_back() {
        DEBUG_ASSERT(this->_size > 0);
        this->destroy_at(this->_size - 1);
        this->_size--;
    }

    void destroy_from(const size_t new_size) {
        DEBUG_ASSERT(new_size <= this->_size);
        while (this->_size > new_size) {
            this->destroy_back();
        }
    }

    alignas(T) std::byte _data[sizeof(T) * N];
    size_t _size = 0;
};
