#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <tl/expected.hpp>

#include "octree/storage/Storage.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/disk/layout/strategy/Default.h"
#include "io/Error.h"

namespace octree {

struct OpenOptions {
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = {};
    std::optional<std::string> preferred_extension_with_dot = {};
};

tl::expected<IndexedStorage, io::Error> open_index(const std::filesystem::path &index_path);
Storage open_folder(
    const std::filesystem::path &base_path,
    const bool create_index = false,
    OpenOptions options = {});
IndexedStorage open_folder_indexed(
    const std::filesystem::path &base_path,
    OpenOptions options = {});

}
