#pragma once

#include <filesystem>

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

