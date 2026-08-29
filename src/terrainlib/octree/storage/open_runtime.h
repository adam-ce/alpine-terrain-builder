#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include <expected>

#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "store/open.h"

namespace octree::storage {

struct OpenOptions {
    std::optional<store::path_layout::Mapping<Id>> default_mapping = std::nullopt;
    std::optional<std::string> preferred_extension = std::nullopt;
};

template <typename NodeData, typename CodecResolver>
Expected<store::Storage<StoreTraits, NodeData>> open_folder(const std::filesystem::path& base_path,
    std::string payload_class,
    std::string default_codec_selector,
    CodecResolver resolve_codec,
    OpenOptions options = {})
{
    std::error_code error;
    const bool base_exists = std::filesystem::exists(base_path, error);
    if (error) {
        return Error::fail(Error::Code::Io, "check existence of", base_path, error);
    }
    if (base_exists && !std::filesystem::is_directory(base_path, error)) {
        return Error::fail(
            Error::Code::Io, "check directory", base_path, error ? error : std::make_error_code(std::errc::not_a_directory));
    }
    if (!base_exists) {
        std::filesystem::create_directories(base_path, error);
        if (error) {
            return Error::fail(Error::Code::Io, "create directories", base_path, error);
        }
    }

    const auto format = index_format();
    const std::filesystem::path index_path = base_path / format.index_filename;
    const std::filesystem::path metadata_path = base_path / metadata_file_name;
    const bool index_exists = std::filesystem::exists(index_path, error);
    if (error) {
        return Error::fail(Error::Code::Io, "check existence of", index_path, error);
    }
    const bool metadata_exists = std::filesystem::exists(metadata_path, error);
    if (error) {
        return Error::fail(Error::Code::Io, "check existence of", metadata_path, error);
    }
    if (index_exists != metadata_exists) {
        return Error::fail(Error::Code::CorruptData,
            "octree dataset metadata and index must either both exist or both be absent: "
                + (index_exists ? metadata_path : index_path).string());
    }
    if (index_exists) {
        auto indexed = store::open_index<StoreTraits, NodeData>(index_path, format, payload_class, resolve_codec);
        if (!indexed) {
            return Error::propagate(
                std::move(indexed), "open indexed octree storage at \"" + base_path.string() + "\"");
        }
        return store::Storage<StoreTraits, NodeData>(std::move(*indexed));
    }

    const store::path_layout::Mapping<Id> mapping = options.default_mapping.value_or(format.default_mapping());
    std::string codec_selector = options.preferred_extension.value_or(std::move(default_codec_selector));
    auto codec = resolve_codec(codec_selector);
    if (!codec) {
        return Error::propagate(std::move(codec), "resolve octree storage codec \"" + codec_selector + "\"");
    }

    return store::make_storage<StoreTraits, NodeData>(base_path, format, mapping, std::move(payload_class), std::move(codec_selector), std::move(*codec));
}

template <typename NodeData, typename CodecResolver>
Expected<store::IndexedStorage<StoreTraits, NodeData>> open_folder_indexed(const std::filesystem::path& base_path,
    std::string payload_class,
    std::string default_codec_selector,
    CodecResolver resolve_codec,
    OpenOptions options = {})
{
    auto result = open_folder<NodeData>(base_path, std::move(payload_class), std::move(default_codec_selector), std::move(resolve_codec), std::move(options));
    if (!result) {
        return Error::propagate(std::move(result));
    }
    return store::IndexedStorage<StoreTraits, NodeData>(std::move(*result));
}

} // namespace octree::storage
