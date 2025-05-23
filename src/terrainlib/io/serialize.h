#pragma once

#include <filesystem>
#include <vector>
#include <span>

#include "io/Error.h"

namespace io {

template <typename T>
tl::expected<std::vector<uint8_t>, Error> write_to_bytes(const T &value);
template <typename T>
tl::expected<T, Error> read_from_bytes(const std::span<const uint8_t> bytes);

template <typename T>
tl::expected<void, Error> write_to_path(const T &value, const std::filesystem::path &path, bool make_dirs = true);
template <typename T>
tl::expected<T, Error> read_from_path(const std::filesystem::path &path);

}

#include "io/serialize.inl"
