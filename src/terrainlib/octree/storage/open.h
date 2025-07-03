#pragma once

#include <filesystem>
#include <memory>

#include <tl/expected.hpp>

#include "octree/storage/Storage.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/disk/layout/strategy/Default.h"
#include "io/Error.h"

namespace octree {

tl::expected<IndexedStorage, io::Error> open_index(const std::filesystem::path &index_path);
Storage open_folder(
    const std::filesystem::path &base_path,
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = disk::layout::strategy::make_default(),
    const std::string extension_with_dot = ".terrain",
    bool create_index = false);
IndexedStorage open_folder_indexed(
    const std::filesystem::path &base_path,
    std::unique_ptr<disk::layout::Strategy> default_layout_strategy = disk::layout::strategy::make_default(),
    const std::string extension_with_dot = ".terrain");

}