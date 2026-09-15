#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "io/envelope.h"
#include "raster_store/StoreTraits.h"
#include "raster_store/Tile.h"
#include "store/IndexFormat.h"

namespace raster_store::io::manifest {

inline constexpr std::string_view metadata_file_name = "raster_store.metadata";
inline constexpr std::string_view index_file_name = "raster_store.index";

namespace detail::v1 {

    struct Metadata {
        std::string layout_id;
        std::string payload_type;
        std::string codec_selector;
        std::uint32_t width = default_tile_side;
        std::uint32_t height = default_tile_side;
    };

    struct IndexEntry {
        std::uint32_t zoom;
        std::uint32_t x;
        std::uint32_t y;
        std::uint8_t status;
    };

    struct Hierarchy {
        std::vector<IndexEntry> entries;
    };

} // namespace detail::v1

using MetadataSchema = ::io::envelope::PayloadSchema<"raster_store.Metadata", ::io::envelope::Version<1, detail::v1::Metadata>>;
using IndexSchema = ::io::envelope::PayloadSchema<"raster_store.Index", ::io::envelope::Version<1, detail::v1::Hierarchy>>;
using Metadata = MetadataSchema::latest_type;

Expected<void> validate(const Metadata& metadata);
Expected<Metadata> read_metadata(const std::filesystem::path& base_path);
Expected<void> write_metadata(const Metadata& metadata, const std::filesystem::path& base_path);
Expected<store::Index<StoreTraits>> read_hierarchy(const std::filesystem::path& index_path);
Expected<store::Index<StoreTraits>> decode_index(const detail::v1::Hierarchy& encoded);
detail::v1::Hierarchy encode_index(const store::Index<StoreTraits>& index);
Expected<store::IndexMetadata<StoreTraits>> read_index_file(const std::filesystem::path& index_path);
Expected<void> write_index_file(const std::filesystem::path& index_path, const store::IndexMetadata<StoreTraits>& metadata);
store::IndexFormat<StoreTraits> index_format();

} // namespace raster_store::io::manifest
