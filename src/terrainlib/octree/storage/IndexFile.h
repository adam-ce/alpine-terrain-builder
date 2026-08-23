#pragma once

#include <filesystem>

#include "io/envelope_file.h"
#include "octree/StoreTraits.h"
#include "octree/storage/Format.h"
#include "octree/store_layout/Mappings.h"
#include "store/IndexFormat.h"

namespace octree::storage {

inline store::IndexFormatError read_error(const std::filesystem::path& path, const io::envelope::FileError& error)
{
    const bool missing = std::holds_alternative<io::Error>(error) && std::get<io::Error>(error) == io::Error(io::Error::OpenFile);
    return {
        missing ? store::IndexFormatErrorCategory::Open : store::IndexFormatErrorCategory::Malformed,
        path,
        io::envelope::describe_error(error),
    };
}

inline std::expected<StoreMetadata, store::IndexFormatError> read_store_metadata(const std::filesystem::path& base_path)
{
    const std::filesystem::path path = base_path / metadata_file_name;
    auto metadata = io::envelope::read_from_path<StoreMetadataSchema>(path);
    if (!metadata) {
        return std::unexpected(read_error(path, metadata.error()));
    }
    if (auto valid = validate(*metadata); !valid) {
        return std::unexpected(store::IndexFormatError {
            store::IndexFormatErrorCategory::Malformed,
            path,
            valid.error(),
        });
    }
    return std::move(*metadata);
}

inline std::expected<store::IndexMetadata<StoreTraits>, store::IndexFormatError> read_index_file(const std::filesystem::path& path)
{
    auto metadata = read_store_metadata(path.parent_path());
    if (!metadata) {
        return std::unexpected(metadata.error());
    }

    auto encoded_index = io::envelope::read_from_path<StoreIndexSchema>(path);
    if (!encoded_index) {
        return std::unexpected(read_error(path, encoded_index.error()));
    }
    auto index = decode_index(*encoded_index);
    if (!index) {
        return std::unexpected(store::IndexFormatError {
            store::IndexFormatErrorCategory::Malformed,
            path,
            index.error(),
        });
    }
    return store::IndexMetadata<StoreTraits> {
        std::move(*index),
        std::move(metadata->layout_id),
        std::move(metadata->payload_class),
        std::move(metadata->codec_selector),
    };
}

inline std::expected<void, store::IndexFormatError> write_index_file(const std::filesystem::path& path, const store::IndexMetadata<StoreTraits>& metadata)
{
    const StoreMetadata store_metadata {
        .layout_id = metadata.layout_id,
        .payload_class = metadata.payload_class,
        .codec_selector = metadata.codec_selector,
    };
    if (auto valid = validate(store_metadata); !valid) {
        return std::unexpected(store::IndexFormatError {
            store::IndexFormatErrorCategory::Write,
            path.parent_path() / metadata_file_name,
            valid.error(),
        });
    }
    const std::filesystem::path metadata_path = path.parent_path() / metadata_file_name;
    if (auto written = io::envelope::write_to_path<StoreMetadataSchema>(store_metadata, metadata_path); !written) {
        return std::unexpected(store::IndexFormatError {
            store::IndexFormatErrorCategory::Write,
            metadata_path,
            io::envelope::describe_error(written.error()),
        });
    }

    const StoreIndex encoded_index = encode_index(metadata.index);
    if (auto written = io::envelope::write_to_path<StoreIndexSchema>(encoded_index, path); !written) {
        return std::unexpected(store::IndexFormatError {
            store::IndexFormatErrorCategory::Write,
            path,
            io::envelope::describe_error(written.error()),
        });
    }
    return {};
}

inline store::PathMapping<Id> default_mapping() { return store_layout::level_and_coordinate_directories(); }

inline store::IndexFormat<StoreTraits> index_format()
{
    return {
        index_file_name,
        read_index_file,
        write_index_file,
        store_layout::from_id,
        default_mapping,
    };
}

} // namespace octree::storage
