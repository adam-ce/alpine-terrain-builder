#pragma once

#include <filesystem>

#include <fmt/format.h>

#include "io/serialize.h"
#include "octree/StoreTraits.h"
#include "octree/disk/IndexFile.h"
#include "octree/store_layout/Mappings.h"
#include "store/IndexFormat.h"

namespace octree::storage {

inline std::expected<store::IndexMetadata<StoreTraits>, store::IndexFormatError>
read_index_file(const std::filesystem::path &path) {
    auto result = io::read_from_path<disk::v1::IndexFile>(path);
    if (!result.has_value()) {
        return std::unexpected(store::IndexFormatError{
            result.error() == io::Error(io::Error::OpenFile)
                ? store::IndexFormatErrorCategory::Open
                : store::IndexFormatErrorCategory::Malformed,
            path,
            fmt::format("{}", result.error()),
        });
    }
    disk::v1::IndexFile file = std::move(result.value());
    return store::IndexMetadata<StoreTraits>{
        std::move(file.map),
        std::move(file.layout_strategy_id),
        std::move(file.preferred_extension),
    };
}

inline std::expected<void, store::IndexFormatError> write_index_file(
    const std::filesystem::path &path,
    const store::IndexMetadata<StoreTraits> &metadata) {
    disk::v1::IndexFile file;
    file.layout_strategy_id = metadata.layout_id;
    file.preferred_extension = metadata.codec_selector;
    file.map = metadata.index;
    const auto result = io::write_to_path(file, path);
    if (!result.has_value()) {
        return std::unexpected(store::IndexFormatError{
            store::IndexFormatErrorCategory::Write,
            path,
            fmt::format("{}", result.error()),
        });
    }
    return {};
}

inline store::PathMapping<Id> default_mapping() {
    return store_layout::level_and_coordinate_directories();
}

inline store::IndexFormat<StoreTraits> index_format() {
    return {
        disk::v1::index_file_name(),
        read_index_file,
        write_index_file,
        store_layout::from_id,
        default_mapping,
    };
}

} // namespace octree::storage
