#pragma once

#include <concepts>
#include <filesystem>
#include <ranges>

#include <tl/expected.hpp>

template <typename Codec, typename T>
concept CodecFor =
    requires(const std::filesystem::path &path, const T &value) {
        typename Codec::value_type;
        typename Codec::load_error;
        typename Codec::save_error;

        requires std::same_as<typename Codec::value_type, T>;

        { Codec::load_from_path(path) } noexcept
            -> std::same_as<tl::expected<T, typename Codec::load_error>>;

        { Codec::save_to_path(value, path) } noexcept
            -> std::same_as<tl::expected<void, typename Codec::save_error>>;

        { Codec::file_not_found() } noexcept
            -> std::same_as<typename Codec::load_error>;

        { Codec::supported_extensions() } -> std::ranges::range;

        requires std::convertible_to<
            std::ranges::range_value_t<decltype(Codec::supported_extensions())>,
            std::string_view>;
    };
