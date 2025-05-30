#pragma once

#include <optional>
#include <utility>
#include <cassert>

template <typename T>
class OnceCell {
public:
    OnceCell() = default;
    OnceCell(T value) {
        this->_value = std::move(value);
    }

    // Returns true if the value was set; false if already initialized
    T& set(T value) {
        if (this->_value.has_value()) {
            return *this->_value;
        }
        this->_value = std::move(value);
        return *this->_value;
    }
    const T& set(T value) const {
        if (this->_value.has_value()) {
            return *this->_value;
        }
        this->_value = std::move(value);
        return *this->_value;
    }

    // Lazily initialize value using init_fn if not already set
    template <typename F>
    const T& get_or_init(F&& init_fn) const {
        if (!this->_value.has_value()) {
            this->_value = init_fn();
        }
        return *this->_value;
    }

    T* get() {
        return this->_value ? &*this->_value : nullptr;
    }
    const T* get() const {
        return this->_value ? &*this->_value : nullptr;
    }

    bool is_initialized() const {
        return this->_value.has_value();
    }

private:
    mutable std::optional<T> _value;
};
