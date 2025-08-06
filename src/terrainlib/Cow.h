#pragma once

#include <functional>
#include <utility>
#include <variant>

template <typename T>
concept NotPtrOrRef = (!std::is_pointer_v<T>) && (!std::is_reference_v<T>);

template <NotPtrOrRef T>
struct Cow {
    using OwnedType = std::remove_const_t<T>;
    using RawRefType = T&;
    using RefType = std::reference_wrapper<T>;
    std::variant<OwnedType, RefType> data;

    // explicit Cow(T&& value)
    //    : data(std::move(value)) {}
    template <typename U = T, typename = std::enable_if_t<std::is_same_v<std::remove_reference_t<U>, T>>>
    explicit Cow(U &&value) : data(std::forward<U>(value)) {}
    explicit Cow(T &ref)
        : data(std::ref(ref)) {}
    template <typename U = T, typename = std::enable_if_t<!std::is_const_v<U>>>
    Cow(Cow<OwnedType>&& other)
        : data(std::visit([](auto &data) -> std::variant<OwnedType, RefType> {
            return data;
        }, other)) {}

    static Cow<T> from_owned(T value) {
        return Cow(std::move(value));
    }
    static Cow<T> from_ref(T &ref) {
        return Cow(ref);
    }

    Cow(const Cow &) = default;
    Cow(Cow &&) noexcept = default;
    Cow &operator=(const Cow &) = default;
    Cow &operator=(Cow &&) noexcept = default;

    bool is_owned() const noexcept {
        return std::holds_alternative<OwnedType>(this->data);
    }
    bool is_ref() const noexcept {
        return std::holds_alternative<RefType>(this->data);
    }

    T &get() {
        return std::visit([](auto &data) -> T& { return data; }, this->data);
    }
    const T &get() const {
        return std::visit([](const auto &data) -> const T& { return data; }, this->data);
    }

    T &operator*() {
        return get();
    }
    const T &operator*() const {
        return get();
    }

    T *operator->() {
        return &get();
    }
    const T *operator->() const {
        return &get();
    }

    operator T &() {
        return get();
    }
    operator const T &() const {
        return get();
    }

    operator Cow<const T>() const {
        if (is_owned()) {
            return Cow<const T>(get());
        } else {
            return Cow<const T>(std::ref(get()));
        }
    }
};

template <typename T>
Cow(T &&) -> Cow<std::remove_cv_t<std::remove_reference_t<T>>>;
