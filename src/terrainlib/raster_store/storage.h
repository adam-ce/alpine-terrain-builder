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

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "raster_store/attribution.h"
#include "raster_store/io/TileCodec.h"
#include "raster_store/io/manifest.h"
#include "raster_store/path_layout.h"
#include "store/open.h"
#include "store/snapshot.h"

namespace raster_store::storage {

template <typename PixelType>
using IndexedStorage = store::IndexedStorage<StoreTraits, Tile<PixelType>>;

struct OpenOptions {
    bool allow_incomplete = false;
};

struct CreateOptions {
    unsigned nominal_tile_size = default_tile_side;
    unsigned halo_width = 0;
    pixel::Placement placement = pixel::Placement::Area;
    unsigned overlap = 0;
    std::optional<pixel::Mapping> value_mapping = std::nullopt;
    std::string codec_selector = "amort";
    ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompression;
    std::optional<std::filesystem::path> copy_attribution_from_index = std::nullopt;
};

namespace detail {

    // The owned handle is destroyed on return, before publication renames its directory.
    template <typename PixelType>
    Expected<void> finish_output(std::unique_ptr<IndexedStorage<PixelType>> storage)
    {
        return storage->save_index();
    }

} // namespace detail

template <typename PixelType>
Expected<std::pair<std::unique_ptr<const IndexedStorage<PixelType>>, std::unique_ptr<const io::manifest::Metadata>>> open(
    const std::filesystem::path& base_path, const OpenOptions options = {})
{
    const auto normalized_path = store::snapshot::normalized(base_path);
    if (!options.allow_incomplete && normalized_path.extension() == ".part") {
        return Error::fail(Error::Code::InvalidInput, "opening an incomplete raster snapshot requires allow_incomplete", base_path);
    }
    auto metadata = io::manifest::read_metadata(normalized_path);
    if (!metadata) {
        return Error::propagate(std::move(metadata), "open raster snapshot");
    }
    const auto index_path = normalized_path / io::manifest::index_file_name;
    auto contents = io::manifest::read_hierarchy(index_path);
    if (!contents) {
        return Error::propagate(std::move(contents), "open raster snapshot");
    }
    auto table = attribution::read_table(index_path);
    if (!table) {
        return Error::propagate(std::move(table), "open raster snapshot attribution");
    }
    const auto resolve_codec
        = [&metadata](const std::string_view name) { return io::TileCodec<PixelType>::from_name(name, glm::uvec2(metadata->stored_tile_size)); };
    auto storage = store::open_index<StoreTraits, Tile<PixelType>>(index_path,
        io::manifest::index_format(),
        { std::move(contents->index), std::move(contents->hashes), metadata->layout_id, metadata->payload_type, metadata->codec_selector },
        pixel::identifier<PixelType>(),
        resolve_codec);
    if (!storage) {
        return Error::propagate(std::move(storage));
    }
    return std::pair { std::make_unique<const IndexedStorage<PixelType>>(std::move(*storage)),
        std::make_unique<const io::manifest::Metadata>(std::move(*metadata)) };
}

template <typename PixelType>
Expected<std::pair<std::unique_ptr<IndexedStorage<PixelType>>, std::unique_ptr<const io::manifest::Metadata>>> create(
    const std::filesystem::path& final_path, const CreateOptions options = {})
{
    const auto destination = store::snapshot::normalized(final_path);
    if (destination.filename().empty() || destination.filename() == "." || destination.filename() == ".." || destination.extension() == ".part") {
        return Error::fail(Error::Code::InvalidInput, "creation requires a final snapshot path without a .part suffix", final_path);
    }
    const auto stored_tile_size = std::uint64_t(options.nominal_tile_size) + options.overlap + 2 * std::uint64_t(options.halo_width);
    if (stored_tile_size > (std::numeric_limits<unsigned>::max)()) {
        return Error::fail(Error::Code::InvalidInput, "stored raster size overflows");
    }
    auto metadata = std::make_unique<const io::manifest::Metadata>(std::string(path_layout::zoom_xy_google::zoom_x_y_google().id),
        pixel::identifier<PixelType>(),
        options.codec_selector,
        unsigned(stored_tile_size),
        options.nominal_tile_size,
        options.halo_width,
        options.value_mapping.value_or(pixel::default_mapping<PixelType>),
        options.placement,
        options.overlap);
    if (auto valid = io::manifest::validate(*metadata); !valid) {
        return Error::propagate(std::move(valid));
    }
    auto codec = io::TileCodec<PixelType>::from_name(options.codec_selector, glm::uvec2(metadata->stored_tile_size), options.compression_algorithm);
    if (!codec) {
        return Error::propagate(std::move(codec));
    }
    // Resolve the table as readers of the published snapshot will.
    auto table = attribution::read_table(options.copy_attribution_from_index.value_or(destination / io::manifest::index_file_name));
    if (!table) {
        return Error::propagate(std::move(table), "resolve attribution for new raster snapshot");
    }
    auto created = store::snapshot::create_partial(destination);
    if (!created) {
        return Error::propagate(std::move(created));
    }
    const auto& partial_path = created->path;
    if (options.copy_attribution_from_index) {
        auto copied = attribution::copy_table(*options.copy_attribution_from_index, partial_path);
        if (!copied) {
            return Error::propagate(std::move(copied));
        }
    }
    auto written = io::manifest::write_metadata(*metadata, partial_path);
    if (!written) {
        return Error::propagate(std::move(written), "create raster metadata");
    }
    auto storage = store::make_storage<StoreTraits, Tile<PixelType>>(partial_path,
        io::manifest::index_format(),
        path_layout::zoom_xy_google::zoom_x_y_google(),
        metadata->payload_type,
        metadata->codec_selector,
        std::move(*codec));
    if (!storage) {
        return Error::propagate(std::move(storage));
    }
    storage->set_snapshot_lock(std::move(created->lock));
    // Even an empty newly created snapshot has a readable checkpoint.
    if (auto saved = storage->save_index(); !saved) {
        return Error::propagate(std::move(saved), "create initial raster index");
    }
    return std::pair { std::make_unique<IndexedStorage<PixelType>>(std::move(*storage)), std::move(metadata) };
}

// Consumes the output handle on success or failure. An unsuccessful publication
// leaves the .part snapshot available for explicit incomplete opening.
template <typename PixelType>
Expected<void> publish(std::unique_ptr<IndexedStorage<PixelType>> storage)
{
    if (!storage) {
        return Error::fail(Error::Code::InvalidInput, "publication requires an output handle");
    }
    const auto partial_path = store::snapshot::normalized(storage->base_path());
    if (!store::snapshot::is_partial(partial_path)) {
        return Error::fail(Error::Code::InvalidInput, "publication requires an incomplete .part snapshot", partial_path);
    }
    // The lock outlives the handle until publication has renamed the attempt and deleted the others.
    auto lock = storage->take_snapshot_lock();
    if (auto finished = detail::finish_output(std::move(storage)); !finished) {
        return Error::propagate(std::move(finished), "finish raster snapshot before publication");
    }
    return store::snapshot::publish(partial_path, std::move(lock));
}

} // namespace raster_store::storage
