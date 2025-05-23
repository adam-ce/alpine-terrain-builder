#pragma once

#include <charconv>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

template <typename T, typename CharT>
std::optional<T> from_chars(std::basic_string_view<CharT> sv) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    if constexpr (std::is_same_v<CharT, char>) {
        T value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc())
            return std::nullopt;
        return value;
    } else {
        std::string utf8(sv.begin(), sv.end());
        return from_chars<T>(std::string_view(utf8));
    }
}

template <typename T, typename CharT>
std::optional<T> from_chars(const std::basic_string<CharT>& s) {
    const std::basic_string_view<CharT> sv = s;
    return from_chars<T, CharT>(sv);
}
