/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
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

#include <filesystem>
#include <glm/glm.hpp>
#include <radix/geometry.h>

#include <fmt/format.h>

namespace fmt {
template <>
struct formatter<std::filesystem::path> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::filesystem::path &path, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(), "{}", std::filesystem::weakly_canonical(path).string());
    }
};

template <glm::length_t N, typename T>
struct formatter<glm::vec<N, T>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec<N, T> &vec, FormatContext &ctx) const {
        auto out = fmt::format_to(ctx.out(), "(");
        for (glm::length_t i = 0; i < N; i++) {
            out = fmt::format_to(out, "{}{}", i == 0 ? "" : ", ", vec[i]);
        }
        return fmt::format_to(out, ")");
    }
};

template <glm::length_t N, typename T>
struct formatter<radix::geometry::Aabb<N, T>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const radix::geometry::Aabb<N, T> &aabb, FormatContext &ctx) const {
        return fmt::format_to(ctx.out(), "[{}-{}]", aabb.min, aabb.max);
    }
};
}
