#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <expected>

#include "Error.h"
#include "store/Index.h"
#include "store/path_layout.h"

namespace store {

template <HierarchyTraits Traits>
struct IndexMetadata {
    Index<Traits> index;
    std::string layout_id;
    std::string payload_class;
    std::string codec_selector;
};

template <HierarchyTraits Traits>
struct IndexFormat {
    std::string_view index_filename;

    Expected<IndexMetadata<Traits>> (*read)(const std::filesystem::path& index_path);
    Expected<void> (*write)(const std::filesystem::path& index_path, const IndexMetadata<Traits>& metadata);
    std::optional<path_layout::Mapping<typename Traits::Key>> (*mapping_from_id)(std::string_view id);
    path_layout::Mapping<typename Traits::Key> (*default_mapping)();
};

} // namespace store
