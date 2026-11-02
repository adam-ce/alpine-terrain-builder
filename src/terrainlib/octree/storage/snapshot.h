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
#include <string>
#include <utility>

#include <expected>

#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "store/IndexedStorage.h"
#include "store/open.h"
#include "store/snapshot.h"

namespace octree::storage {

// Creates an empty storage in a new attempt of final_path; see store::snapshot.
template <typename NodeData, typename CodecResolver>
Expected<store::IndexedStorage<StoreTraits, NodeData>> create(const std::filesystem::path& final_path,
    std::string payload_class,
    std::string codec_selector,
    CodecResolver resolve_codec,
    const store::path_layout::Mapping<Id> mapping)
{
    auto codec = resolve_codec(codec_selector);
    if (!codec) {
        return Error::propagate(std::move(codec), "resolve octree storage codec \"" + codec_selector + "\"");
    }
    auto attempt = store::snapshot::create_partial(final_path);
    if (!attempt) {
        return Error::propagate(std::move(attempt));
    }
    auto storage = store::make_storage<StoreTraits, NodeData>(
        attempt->path, index_format(), mapping, std::move(payload_class), std::move(codec_selector), std::move(*codec));
    if (!storage) {
        return Error::propagate(std::move(storage));
    }
    storage->set_snapshot_lock(std::move(attempt->lock));
    // Even an empty incomplete snapshot has a readable checkpoint.
    if (auto saved = storage->save_index(); !saved) {
        return Error::propagate(std::move(saved), "create initial octree index");
    }
    return store::IndexedStorage<StoreTraits, NodeData>(std::move(*storage));
}

// Saves the index and publishes the attempt to its final path; see store::snapshot.
template <typename NodeData>
Expected<void> publish(store::IndexedStorage<StoreTraits, NodeData> storage)
{
    if (auto saved = storage.save_index(); !saved) {
        return Error::propagate(std::move(saved), "save octree index before publication");
    }
    return store::snapshot::publish(storage.base_path(), storage.take_snapshot_lock());
}

} // namespace octree::storage
