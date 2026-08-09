#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <optional>

template <typename T>
class TinyVector {
public:
    TinyVector() = default;

    template <typename U>
    void push_back(U &&value) {
        this->emplace_back(std::forward<U>(value));
    }

    template <typename... Args>
    T &emplace_back(Args &&...args) {
        return this->visit(
            [&](Inline &option) -> T & {
                if (!option.has_value()) {
                    return option.emplace(std::forward<Args>(args)...);
                }

                return this->promote_to_vector(2)
                    .emplace_back(std::forward<Args>(args)...);
            },
            [&](Vector &vector) -> T & {
                return vector.emplace_back(std::forward<Args>(args)...);
            });
    }

    void pop_back() {
        if (this->empty()) {
            throw std::out_of_range("TinyVector::pop_back on empty");
        }

        this->resize(this->size() - 1);
    }

    void reserve(const size_t new_capacity) {
        if (new_capacity > 1) {
            this->promote_to_vector(new_capacity);
        }
    }

    void clear() {
        this->resize(0);
    }

    void resize(const size_t new_size) {
        this->visit(
            [&](Inline &option) {
                if (new_size == 0) {
                    option.reset();
                } else if (new_size == 1 && !option.has_value()) {
                    option.emplace();
                } else if (new_size > 1) {
                    this->promote_to_vector(new_size).resize(new_size);
                }
            },
            [&](Vector &vector) {
                vector.resize(new_size);
            });
    }

    T *data() {
        return this->data_impl(*this);
    }

    const T *data() const {
        return this->data_impl(*this);
    }

    size_t size() const noexcept {
        return this->visit(
            [](const Inline &option) -> size_t {
                return option.has_value() ? 1 : 0;
            },
            [](const Vector &vector) -> size_t {
                return vector.size();
            });
    }

    bool empty() const noexcept {
        return this->size() == 0;
    }

    T &operator[](const size_t index) {
        return this->data()[index];
    }

    const T &operator[](const size_t index) const {
        return this->data()[index];
    }

    T &at(const size_t index) {
        this->check_index(index);
        return (*this)[index];
    }

    const T &at(const size_t index) const {
        this->check_index(index);
        return (*this)[index];
    }

    T *begin() {
        return this->data();
    }

    T *end() {
        return this->data() + this->size();
    }

    const T *begin() const {
        return this->data();
    }

    const T *end() const {
        return this->data() + this->size();
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

private:
    using Inline = std::optional<T>;
    using Vector = std::vector<T>;
    using Storage = std::variant<Inline, Vector>;

    template <typename InlineFn, typename VectorFn>
    decltype(auto) visit(InlineFn &&inline_fn, VectorFn &&vector_fn) {
        return this->visit_impl(
            *this,
            std::forward<InlineFn>(inline_fn),
            std::forward<VectorFn>(vector_fn));
    }

    template <typename InlineFn, typename VectorFn>
    decltype(auto) visit(InlineFn &&inline_fn, VectorFn &&vector_fn) const {
        return this->visit_impl(
            *this,
            std::forward<InlineFn>(inline_fn),
            std::forward<VectorFn>(vector_fn));
    }

    template <typename Self, typename InlineFn, typename VectorFn>
    static decltype(auto) visit_impl(
        Self &self,
        InlineFn &&inline_fn,
        VectorFn &&vector_fn) {
        return std::visit(
            [&](auto &value) -> decltype(auto) {
                using Value = std::remove_cvref_t<decltype(value)>;

                if constexpr (std::is_same_v<Value, Inline>) {
                    return std::forward<InlineFn>(inline_fn)(value);
                } else {
                    return std::forward<VectorFn>(vector_fn)(value);
                }
            },
            self._storage);
    }

    template <typename Self>
    static auto data_impl(Self &self) {
        constexpr bool is_const = std::is_const_v<std::remove_reference_t<Self>>;
        using Pointer = std::conditional_t<is_const, const T *, T *>;

        return self.visit(
            [](auto &option) -> Pointer {
                if (option.has_value()) {
                    return &option.value();
                } else {
                    return nullptr;
                }
            },
            [](auto &vector) -> Pointer {
                return vector.data();
            });
    }

    Vector &promote_to_vector(const size_t min_capacity) {
        return this->visit(
            [&](Inline &option) -> Vector & {
                Vector vector;
                vector.reserve(min_capacity);

                if (option.has_value()) {
                    vector.push_back(std::move(option.value()));
                }

                this->_storage = std::move(vector);
                return std::get<Vector>(this->_storage);
            },
            [&](Vector &vector) -> Vector & {
                vector.reserve(min_capacity);
                return vector;
            });
    }

    void check_index(const size_t index) const {
        if (index >= this->size()) {
            throw std::out_of_range("TinyVector::at");
        }
    }

    Storage _storage;
};
