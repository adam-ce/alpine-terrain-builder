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
