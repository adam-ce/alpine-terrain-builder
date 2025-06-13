#pragma once

#include <filesystem>
#include <glm/gtx/string_cast.hpp>

#include <fmt/format.h>

template <>
struct fmt::formatter<std::filesystem::path> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::filesystem::path &path, FormatContext &ctx) {
        return fmt::format_to(ctx.out(), "{}", std::filesystem::weakly_canonical(path).string());
    }
};

template <glm::length_t N, typename T>
struct fmt::formatter<glm::vec<N, T>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const glm::vec<N, T> &vec, FormatContext &ctx) {
        return fmt::format_to(ctx.out(), "{}", glm::to_string(vec));
    }
};
