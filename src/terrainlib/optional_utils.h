#pragma once

#include <optional>

template <typename T>
std::optional<std::reference_wrapper<T>> as_ref(const std::optional<T> &option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}

template <typename T, typename Func>
auto map(const std::optional<T> &option, Func &&f) -> std::optional<std::invoke_result_t<Func, T>> {
    using U = std::invoke_result_t<Func, T>;

    if (option.has_value()) {
        return std::optional<U>{f(option.value())};
    } else {
        return std::nullopt;
    }
}

template <typename T>
std::optional<T> flatten(const std::optional<std::optional<T>> option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}
