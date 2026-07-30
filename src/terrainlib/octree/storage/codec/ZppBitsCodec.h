#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include <tl/expected.hpp>

#include "Codec.h"
#include "io/serialize.h"
#include "io/Error.h"

namespace octree {

template <typename T>
struct ZppBitsCodec {
    using value_type = T;
    using load_error = io::Error;
    using save_error = io::Error;

    static tl::expected<value_type, load_error> load_from_path(const std::filesystem::path& path) noexcept {
        return io::read_from_path<value_type>(path);
    }

    static tl::expected<void, save_error> save_to_path(const value_type& value, const std::filesystem::path& path) noexcept {
        return io::write_to_path(value, path);
    }

    static load_error file_not_found() noexcept {
        return io::Error::Value::OpenFile;
    }

    static std::vector<std::string_view> supported_extensions() {
        return {".bin"};
    }
};

} // namespace octree
