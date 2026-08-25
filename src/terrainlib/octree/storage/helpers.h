#pragma once

#include <memory>
#include <optional>
#include <filesystem>

#include <expected>

#include "io/Error.h"
#include "octree/IndexMap.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/Strategy.h"

namespace octree::helpers {

struct LayoutWithoutBase {
    std::unique_ptr<disk::layout::Strategy> strategy;
    std::string extension_with_dot;
};

std::optional<LayoutWithoutBase> guess_layout_strategy(
    const std::filesystem::path &base_path,
    size_t max_files_to_check = 100);
std::expected<void, io::Error> save_index_map(const IndexMap &index, const disk::Layout &layout);
void update_index_map(IndexMap &index, const disk::Layout &layout);

}
