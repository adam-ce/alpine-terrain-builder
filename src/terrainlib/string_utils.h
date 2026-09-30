/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 * Copyright (C) 2025 Adrian Gawor
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
#include <charconv>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

// Format a byte count as a human-readable string, e.g. "1.5 GiB".
inline std::string human_byte_size(const size_t bytes) {
    constexpr std::array<const char *, 5> units = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < units.size()) {
        value /= 1024.0;
        unit++;
    }
    std::array<char, 32> buffer;
    const char *format = unit == 0 ? "%.0f %s" : "%.1f %s";
    std::snprintf(buffer.data(), buffer.size(), format, value, units[unit]);
    return std::string(buffer.data());
}

template <typename T, typename CharT>
std::optional<T> from_chars(const std::basic_string_view<CharT> sv) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    if constexpr (std::is_same_v<CharT, char>) {
        T value;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc() || ptr != sv.data() + sv.size())
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
