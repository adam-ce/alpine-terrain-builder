#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <expected>

#include "store/Index.h"
#include "store/PathMapping.h"

namespace store {

enum class IndexFormatErrorCategory {
    Open,
    Write,
    Malformed,
};

struct IndexFormatError {
    IndexFormatErrorCategory category;
    std::filesystem::path path;
    std::string message;

    bool operator==(const IndexFormatError &) const = default;
};

template<HierarchyTraits Traits>
struct IndexMetadata {
    Index<Traits> index;
    std::string layout_id;
    std::string payload_class;
    std::string codec_selector;
};

template<HierarchyTraits Traits>
struct IndexFormat {
    std::string_view index_filename;

    std::expected<IndexMetadata<Traits>, IndexFormatError> (*read)(
        const std::filesystem::path &index_path);
    std::expected<void, IndexFormatError> (*write)(
        const std::filesystem::path &index_path,
        const IndexMetadata<Traits> &metadata);
    std::optional<PathMapping<typename Traits::Key>> (*mapping_from_id)(std::string_view id);
    PathMapping<typename Traits::Key> (*default_mapping)();
};

} // namespace store
