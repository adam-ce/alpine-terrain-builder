/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
