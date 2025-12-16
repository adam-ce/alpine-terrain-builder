#pragma once

#include <algorithm>
#include <initializer_list>
#include <new>
#include <span>
#include <stdexcept>

#include <libassert/assert.hpp>

template <typename T, std::size_t N>
class FixedVector {
public:
    constexpr FixedVector() = default;

    FixedVector(std::initializer_list<T> init) {
        if (init.size() > N) {
            throw std::out_of_range("capacity exceeded");
        }
        for (const T &value : init) {
            this->construct_at(this->_size, value);
            this->_size++;
        }
    }
    
    // Copy constructor
    FixedVector(const FixedVector &other) {
        for (size_t i = 0; i < other._size; ++i) {
            this->construct_at(i, other[i]);
        }
        this->_size = other._size;
    }

    // Copy assignment
    FixedVector &operator=(const FixedVector &other) {
        if (this == &other) {
            return *this;
        }

        // Destroy extra elements if needed
        for (size_t i = other._size; i < this->_size; ++i) {
            this->destroy_at(i);
        }

        // Copy-assign existing elements
        size_t i = 0;
        for (; i < std::min(this->_size, other._size); ++i) {
            (*this)[i] = other[i];
        }

        // Copy-construct new elements if other is bigger
        for (; i < other._size; ++i) {
            this->construct_at(i, other[i]);
        }

        this->_size = other._size;
        return *this;
    }

    // Move constructor
    FixedVector(FixedVector &&other) noexcept {
        for (size_t i = 0; i < other._size; ++i) {
            this->construct_at(i, std::move(other[i]));
        }
        this->_size = other._size;
        other.clear();
    }

    // Move assignment
    FixedVector &operator=(FixedVector &&other) noexcept {
        if (this == &other) {
            return *this;
        }

        // Destroy extra elements if needed
        for (size_t i = other._size; i < this->_size; ++i) {
            this->destroy_at(i);
        }

        // Move-assign existing elements
        size_t i = 0;
        for (; i < std::min(this->_size, other._size); ++i) {
            (*this)[i] = std::move(other[i]);
        }

        // Move-construct new elements if other is bigger
        for (; i < other._size; ++i) {
            this->construct_at(i, std::move(other[i]));
        }

        this->_size = other._size;
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
        this->destroy_at(this->_size - 1);
        this->_size--;
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
        return reinterpret_cast<T *>(this->_data);
    }
    const T *data() const {
        return reinterpret_cast<const T *>(this->_data);
    }

    void clear() {
        for (size_t i = this->_size; i-- > 0;) {
            this->destroy_at(i);
        }
        this->_size = 0;
    }

    void resize(const size_t new_size) {
        if (new_size > N) {
            throw std::out_of_range("capacity exceeded");
        }

        if (this->_size > new_size) {
            for (size_t i = new_size; i < this->_size; i++) {
                this->destroy_at(i);
            }
        } else {
            for (size_t i = this->_size; i < new_size; i++) {
                this->construct_at(i);
            }
        }
        this->_size = new_size;
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
    const T *cbegin() const {
        return this->begin();
    }
    const T *cend() const {
        return this->end();
    }

    operator std::span<T>() {
        return std::span<T>(this->data(), this->size());
    }

    operator std::span<const T>() const {
        return std::span<const T>(this->data(), this->size());
    }

    ~FixedVector() {
        for (size_t i = this->_size; i-- > 0;) {
            this->destroy_at(i);
        }
    }

private:
    T &get_at(const size_t index) {
        DEBUG_ASSERT(index < this->_size);
        return *std::launder(reinterpret_cast<T *>(&this->_data[index]));
    }
    const T &get_at(const size_t index) const {
        DEBUG_ASSERT(index < this->_size);
        return *std::launder(reinterpret_cast<const T *>(&this->_data[index]));
    }

    template <typename... Args>
    void construct_at(const size_t index, Args &&...args) {
        DEBUG_ASSERT(index >= this->_size && index < this->capacity());
        ::new (&this->_data[index]) T(std::forward<Args>(args)...);
    }

    void destroy_at(const size_t index) {
        DEBUG_ASSERT(index < this->_size);
        std::destroy_at(std::launder(reinterpret_cast<T *>(&this->_data[index])));
    }

    std::aligned_storage_t<sizeof(T), alignof(T)> _data[N];
    size_t _size = 0;
};
