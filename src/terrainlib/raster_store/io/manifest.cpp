#include "raster_store/io/manifest.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <system_error>
#include <unordered_set>

#include "raster_store/path_layout.h"

namespace raster_store::io::manifest {

Expected<void> validate(const Metadata& metadata)
{
    if (metadata.layout_id.empty() || metadata.payload_type.empty() || metadata.codec_selector.empty()) {
        return Error::fail(Error::Code::InvalidInput, "raster metadata requires layout, payload type, and codec selector");
    }
    if (!std::has_single_bit(metadata.nominal_tile_size) || metadata.halo_width > metadata.nominal_tile_size
        || metadata.halo_width > ((std::numeric_limits<unsigned>::max)() - metadata.nominal_tile_size) / 2
        || metadata.stored_tile_size != metadata.nominal_tile_size + 2 * metadata.halo_width) {
        return Error::fail(Error::Code::InvalidInput, "invalid raster nominal size, stored size, or halo width");
    }
    if (metadata.value_mapping != pixel::Mapping::Linear && metadata.value_mapping != pixel::Mapping::SRGBA) {
        return Error::fail(Error::Code::InvalidInput, "unknown raster value mapping");
    }
    return {};
}

Expected<Metadata> read_metadata(const std::filesystem::path& base_path)
{
    auto metadata = ::io::envelope::read_from_path<MetadataSchema>(base_path / metadata_file_name);
    if (!metadata) {
        return metadata;
    }
    if (auto valid = validate(*metadata); !valid) {
        return Error::propagate(std::move(valid), Error::Code::CorruptData, "validate raster metadata");
    }
    return metadata;
}

Expected<void> write_metadata(const Metadata& metadata, const std::filesystem::path& base_path)
{
    return ::io::envelope::write_to_path<MetadataSchema>(metadata, base_path / metadata_file_name);
}

detail::v1::Hierarchy encode_index(const store::Index<StoreTraits>& index)
{
    detail::v1::Hierarchy encoded;
    encoded.entries.reserve(index.size());
    for (const auto& [key, status] : index) {
        encoded.entries.push_back({ key, status });
    }
    std::ranges::sort(encoded.entries, [](const auto& left, const auto& right) { return left.id < right.id; });
    return encoded;
}

Expected<store::Index<StoreTraits>> decode_index(const detail::v1::Hierarchy& encoded)
{
    store::Index<StoreTraits> index;
    std::unordered_set<radix::tile::Id, StoreTraits::Hasher> seen;
    for (const auto& entry : encoded.entries) {
        if (!StoreTraits::is_valid(entry.id)) {
            return Error::fail(Error::Code::CorruptData, "raster index contains an invalid tile ID");
        }
        if (!seen.insert(entry.id).second) {
            return Error::fail(Error::Code::CorruptData, "raster index contains a duplicate tile ID");
        }
        if (entry.status > store::NodeStatus::Virtual) {
            return Error::fail(Error::Code::CorruptData, "raster index contains an invalid node status");
        }
        auto added = index.set_raw(entry.id, store::NodeStatus { entry.status });
        if (!added) {
            return Error::propagate(std::move(added), Error::Code::CorruptData, "decode raster index entry");
        }
    }
    if (auto valid = index.validate(); !valid) {
        return Error::propagate(std::move(valid), "validate raster index topology");
    }
    return index;
}

Expected<store::Index<StoreTraits>> read_hierarchy(const std::filesystem::path& index_path)
{
    auto encoded = ::io::envelope::read_from_path<IndexSchema>(index_path);
    if (!encoded) {
        return Error::propagate(std::move(encoded), "read raster hierarchy");
    }
    return decode_index(*encoded);
}

Expected<store::IndexMetadata<StoreTraits>> read_index_file(const std::filesystem::path& index_path)
{
    auto metadata = read_metadata(index_path.parent_path());
    if (!metadata) {
        return Error::propagate(std::move(metadata));
    }
    auto index = read_hierarchy(index_path);
    if (!index) {
        return Error::propagate(std::move(index));
    }
    return store::IndexMetadata<StoreTraits> { std::move(*index), metadata->layout_id, metadata->payload_type, metadata->codec_selector };
}

Expected<void> write_index_file(const std::filesystem::path& index_path, const store::IndexMetadata<StoreTraits>& metadata)
{
    auto temporary_path = index_path;
    temporary_path += ".tmp";
    auto written = ::io::envelope::write_to_path<IndexSchema>(encode_index(metadata.index), temporary_path);
    if (!written) {
        return Error::propagate(std::move(written), "checkpoint raster index");
    }
    std::error_code error;
    std::filesystem::rename(temporary_path, index_path, error);
    if (error) {
        return Error::fail(Error::Code::Io, "replace raster index checkpoint", temporary_path, index_path, error);
    }
    return {};
}

store::IndexFormat<StoreTraits> index_format()
{
    return { index_file_name, read_index_file, write_index_file, path_layout::zoom_xy_google::from_id, path_layout::zoom_xy_google::zoom_x_y_google };
}

} // namespace raster_store::io::manifest
