#pragma once

#include <expected>
#include <filesystem>

#include "log.h"

namespace octree {

// Wraps a codec so saving always fails loudly instead of writing, for storages that must stay
// read-only (e.g. a view that aliases another storage's files).
template <typename Codec>
struct ReadOnlyCodec : Codec {
    static std::expected<void, typename Codec::save_error> save_to_path(
        const typename Codec::value_type &, const std::filesystem::path &) noexcept {
        LOG_ERROR_AND_EXIT("Attempted to write through a read-only codec");
    }
};

} // namespace octree
