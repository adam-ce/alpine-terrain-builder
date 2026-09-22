#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "io/utils.h"
#include "raster_store/attribution.h"
#include "raster_store/io/TileCodec.h"
#include "raster_store/io/manifest.h"
#include "raster_store/path_layout.h"
#include "store/open.h"

namespace raster_store::storage {

template <typename PixelType>
using IndexedStorage = store::IndexedStorage<StoreTraits, Tile<PixelType>>;

struct OpenOptions {
    bool allow_incomplete = false;
};

struct CreateOptions {
    glm::uvec2 tile_dimensions { default_tile_side };
    std::string codec_selector = "amort";
    ::io::envelope::CompressionAlgorithm compression_algorithm = ::io::envelope::CompressionAlgorithm::ZstdDefaultCompressionWithChecksum;
    ::io::envelope::ChecksumAlgorithm checksum_algorithm = ::io::envelope::ChecksumAlgorithm::HandledByCompressionLib;
    std::optional<std::filesystem::path> copy_attribution_from_index;
};

namespace detail {

    inline Expected<void> require_absent(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(path, error);
        if (error && error != std::errc::no_such_file_or_directory) {
            return Error::fail(Error::Code::Io, "inspect snapshot destination", path, error);
        }
        if (std::filesystem::exists(status)) {
            return Error::fail(Error::Code::AlreadyExists, "create snapshot at", path);
        }
        return {};
    }

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
    auto normalized_path = base_path.lexically_normal();
    if (!normalized_path.has_filename()) {
        normalized_path = normalized_path.parent_path();
    }
    if (!options.allow_incomplete && normalized_path.extension() == ".part") {
        return Error::fail(Error::Code::InvalidInput, "opening an incomplete raster snapshot requires allow_incomplete", base_path);
    }
    auto metadata = io::manifest::read_metadata(normalized_path);
    if (!metadata) {
        return Error::propagate(std::move(metadata), "open raster snapshot");
    }
    const auto index_path = normalized_path / io::manifest::index_file_name;
    auto index = io::manifest::read_hierarchy(index_path);
    if (!index) {
        return Error::propagate(std::move(index), "open raster snapshot");
    }
    auto table = attribution::read_table(index_path);
    if (!table) {
        return Error::propagate(std::move(table), "open raster snapshot attribution");
    }
    const auto resolve_codec = [&metadata](const std::string_view name) {
        return io::tile_codec::from_name<PixelType>(name, { metadata->width, metadata->height });
    };
    auto storage = store::open_index<StoreTraits, Tile<PixelType>>(index_path,
        io::manifest::index_format(),
        { std::move(*index), metadata->layout_id, metadata->payload_type, metadata->codec_selector },
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
    const auto destination = final_path.lexically_normal();
    if (destination.filename().empty() || destination.filename() == "." || destination.filename() == ".." || destination.extension() == ".part") {
        return Error::fail(Error::Code::InvalidInput, "creation requires a final snapshot path without a .part suffix", final_path);
    }
    if (auto valid = validate_dimensions(options.tile_dimensions); !valid) {
        return Error::propagate(std::move(valid));
    }
    auto codec = io::tile_codec::from_name<PixelType>(
        options.codec_selector, options.tile_dimensions, options.compression_algorithm, options.checksum_algorithm);
    if (!codec) {
        return Error::propagate(std::move(codec));
    }
    if (auto absent = detail::require_absent(destination); !absent) {
        return Error::propagate(std::move(absent));
    }
    auto partial_path = destination;
    partial_path += ".part";
    if (auto absent = detail::require_absent(partial_path); !absent) {
        return Error::propagate(std::move(absent));
    }
    auto table = attribution::read_table(options.copy_attribution_from_index.value_or(partial_path / io::manifest::index_file_name));
    if (!table) {
        return Error::propagate(std::move(table), "resolve attribution for new raster snapshot");
    }
    std::error_code error;
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path(), error);
        if (error) {
            return Error::fail(Error::Code::Io, "create snapshot parent directory", destination.parent_path(), error);
        }
    }
    if (!std::filesystem::create_directory(partial_path, error)) {
        return Error::fail(error ? Error::Code::Io : Error::Code::AlreadyExists, "create incomplete snapshot", partial_path, error);
    }
    if (options.copy_attribution_from_index) {
        auto copied = attribution::copy_table(*options.copy_attribution_from_index, partial_path);
        if (!copied) {
            return Error::propagate(std::move(copied));
        }
    }
    auto metadata = std::make_unique<const io::manifest::Metadata>(std::string(path_layout::zoom_xy_google::zoom_x_y_google().id),
        pixel::identifier<PixelType>(),
        options.codec_selector,
        options.tile_dimensions.x,
        options.tile_dimensions.y);
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
    const auto partial_path = storage->base_path().lexically_normal();
    if (partial_path.extension() != ".part") {
        return Error::fail(Error::Code::InvalidInput, "publication requires an incomplete .part snapshot", partial_path);
    }
    auto destination = partial_path;
    destination.replace_extension();
    if (auto absent = detail::require_absent(destination); !absent) {
        return absent;
    }
    if (auto finished = detail::finish_output(std::move(storage)); !finished) {
        return Error::propagate(std::move(finished), "finish raster snapshot before publication");
    }
    auto renamed = ::io::utils::rename_without_replacement(partial_path, destination);
    if (!renamed) {
        return Error::propagate(std::move(renamed), "publish raster snapshot");
    }
    return {};
}

} // namespace raster_store::storage
