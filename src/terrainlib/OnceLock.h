#pragma once

#include <optional>
#include <mutex>
#include <functional>
#include <utility>

template <typename T>
class OnceLock {
public:
    OnceLock() = default;
    OnceLock(const OnceLock&) = delete;
    OnceLock& operator=(const OnceLock&) = delete;

    // Initialize with a value (if not already set)
    bool set(T value) {
        if (this->is_initialized()) {
            return false; // Already initialized
        }
        bool initialized = false;
        std::call_once(this->_init_flag, [this, &value, &initialized]() {
            this->_value.emplace(std::move(value));
            initialized = true;
        });
        return initialized;
    }

    // Lazily initialize using a function
    template <typename F>
    const T& get_or_init(F&& init_fn) const {
        std::call_once(this->_init_flag, [this, &init_fn]() {
            this->_value.emplace(init_fn());
        });
        return *this->_value;
    }

    // Get without initializing (returns nullptr if uninitialized)
    const T* get() const {
        return this->_value ? &*this->_value : nullptr;
    }

    bool is_initialized() const {
        return this->_value.has_value();
    }

private:
    mutable std::optional<T> _value;
    mutable std::once_flag _init_flag;
};
