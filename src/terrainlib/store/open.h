#pragma once

#include <filesystem>
#include <memory>
#include <utility>

#include <expected>

#include "store/IndexedStorage.h"
#include "store/OpenError.h"

namespace store {

template<HierarchyTraits Traits, typename NodeData, typename CodecResolver>
std::expected<IndexedStorage<Traits, NodeData>, OpenError<typename Traits::Key>> open_index(
    const std::filesystem::path &index_path,
    const IndexFormat<Traits> format,
    const std::string_view expected_payload_class,
    CodecResolver &&resolve_codec) {
    using Key = typename Traits::Key;
    const auto metadata_result = format.read(index_path);
    if (!metadata_result.has_value()) {
        return std::unexpected(OpenError<Key>(metadata_result.error()));
    }
    IndexMetadata<Traits> metadata = std::move(metadata_result.value());
    if (metadata.payload_class != expected_payload_class) {
        return std::unexpected(OpenError<Key>(CodecError{
            CodecOperation::Resolve,
            CodecErrorCategory::UnsupportedCodec,
            "unexpected payload class: " + metadata.payload_class,
        }));
    }
    for (const auto &[key, status] : metadata.index) {
        static_cast<void>(status);
        if (!Traits::is_valid(key)) {
            return std::unexpected(OpenError<Key>(InvalidKey<Key>{key}));
        }
    }

    const auto mapping = format.mapping_from_id(metadata.layout_id);
    if (!mapping.has_value()) {
        return std::unexpected(OpenError<Key>(UnknownLayout{metadata.layout_id}));
    }
    auto codec = resolve_codec(metadata.codec_selector);
    if (!codec.has_value()) {
        return std::unexpected(OpenError<Key>(codec.error()));
    }

    const std::filesystem::path base_path = index_path.parent_path();
    using Storage = store::Storage<Traits, NodeData>;
    return IndexedStorage<Traits, NodeData>(
        RawStorage<Traits, NodeData>(
            Layout<Key>(base_path, mapping.value()),
            std::move(codec.value())),
        std::move(metadata.index),
        typename Storage::Persistence{
            format,
            index_path,
            std::move(metadata.layout_id),
            std::move(metadata.payload_class),
            std::move(metadata.codec_selector),
        });
}

template<HierarchyTraits Traits, typename NodeData>
std::expected<Storage<Traits, NodeData>, OpenError<typename Traits::Key>> make_storage(
    const std::filesystem::path &base_path,
    const IndexFormat<Traits> format,
    const PathMapping<typename Traits::Key> mapping,
    std::string payload_class,
    std::string codec_selector,
    std::unique_ptr<Codec<NodeData>> codec) {
    using Key = typename Traits::Key;
    std::error_code error;
    std::filesystem::create_directories(base_path, error);
    if (error) {
        return std::unexpected(OpenError<Key>(FilesystemError{
            base_path,
            "create_directories",
            error,
        }));
    }

    typename Storage<Traits, NodeData>::Persistence persistence{
        format,
        base_path / format.index_filename,
        std::string(mapping.id),
        std::move(payload_class),
        std::move(codec_selector),
    };
    RawStorage<Traits, NodeData> raw(
        Layout<Key>(base_path, mapping),
        std::move(codec));
    return Storage<Traits, NodeData>(
        std::move(raw),
        Index<Traits>{},
        std::move(persistence),
        true);
}

} // namespace store
