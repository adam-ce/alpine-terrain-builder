#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

#include <expected>

#include "octree/StoreTraits.h"
#include "octree/storage/IndexFile.h"
#include "store/open.h"

namespace octree::storage {

struct OpenOptions {
    std::optional<store::PathMapping<Id>> default_mapping;
    std::optional<std::string> preferred_extension;
};

struct DiscoveredLayout {
    store::PathMapping<Id> mapping;
    std::string codec_selector;
};

template<typename NodeData, typename CodecResolver>
std::optional<DiscoveredLayout> discover_layout(
    const std::filesystem::path &base_path,
    CodecResolver &resolve_codec) {
    struct Candidate {
        std::filesystem::path path;
        std::string extension;
    };
    std::vector<Candidate> candidates;
    std::unordered_map<std::string, size_t> extension_counts;
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(base_path, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        if (!iterator->is_regular_file(error) && !iterator->is_symlink(error)) {
            if (error) {
                break;
            }
            continue;
        }
        if (iterator->path().filename() == disk::v1::index_file_name()) {
            continue;
        }
        const std::string extension = iterator->path().extension().string();
        if (extension.empty() || !resolve_codec(extension).has_value()) {
            continue;
        }
        candidates.push_back({iterator->path(), extension});
        ++extension_counts[extension];
    }
    if (error || candidates.empty()) {
        return std::nullopt;
    }

    std::optional<store::PathMapping<Id>> best_mapping;
    size_t best_match_count = 0;
    for (const auto mapping : store_layout::all()) {
        const store::Layout<Id> layout(base_path, mapping);
        size_t matches = 0;
        for (const Candidate &candidate : candidates) {
            std::filesystem::path node_path = candidate.path;
            node_path.replace_extension();
            matches += layout.key_from_node_path(store::NodePath(node_path)).has_value();
        }
        if (matches > best_match_count) {
            best_mapping = mapping;
            best_match_count = matches;
        }
    }
    if (!best_mapping.has_value()) {
        return std::nullopt;
    }

    const auto extension = std::max_element(
        extension_counts.begin(),
        extension_counts.end(),
        [](const auto &left, const auto &right) { return left.second < right.second; });
    return DiscoveredLayout{best_mapping.value(), extension->first};
}

template<typename NodeData, typename CodecResolver>
std::expected<store::Storage<StoreTraits, NodeData>, store::OpenError<Id>> open_folder(
    const std::filesystem::path &base_path,
    const bool create_index,
    std::string default_codec_selector,
    CodecResolver resolve_codec,
    OpenOptions options = {}) {
    std::error_code error;
    const bool base_exists = std::filesystem::exists(base_path, error);
    if (error) {
        return std::unexpected(store::OpenError<Id>(store::FilesystemError{
            base_path,
            "exists",
            error,
        }));
    }
    if (base_exists && !std::filesystem::is_directory(base_path, error)) {
        return std::unexpected(store::OpenError<Id>(store::FilesystemError{
            base_path,
            "is_directory",
            error ? error : std::make_error_code(std::errc::not_a_directory),
        }));
    }
    if (!base_exists) {
        std::filesystem::create_directories(base_path, error);
        if (error) {
            return std::unexpected(store::OpenError<Id>(store::FilesystemError{
                base_path,
                "create_directories",
                error,
            }));
        }
    }

    const auto format = index_format();
    const std::filesystem::path index_path = base_path / format.index_filename;
    const bool index_exists = std::filesystem::exists(index_path, error);
    if (error) {
        return std::unexpected(store::OpenError<Id>(store::FilesystemError{
            index_path,
            "exists",
            error,
        }));
    }
    if (index_exists) {
        auto indexed = store::open_index<StoreTraits, NodeData>(
            index_path,
            format,
            resolve_codec);
        if (!indexed.has_value()) {
            return std::unexpected(indexed.error());
        }
        return store::Storage<StoreTraits, NodeData>(std::move(indexed.value()));
    }

    const auto discovered = discover_layout<NodeData>(base_path, resolve_codec);
    const store::PathMapping<Id> mapping = discovered.has_value()
        ? discovered->mapping
        : options.default_mapping.value_or(format.default_mapping());
    std::string codec_selector = discovered.has_value()
        ? discovered->codec_selector
        : options.preferred_extension.value_or(std::move(default_codec_selector));
    auto codec = resolve_codec(codec_selector);
    if (!codec.has_value()) {
        return std::unexpected(store::OpenError<Id>(codec.error()));
    }

    auto storage_result = store::make_storage<StoreTraits, NodeData>(
        base_path,
        format,
        mapping,
        std::move(codec_selector),
        std::move(codec.value()),
        false);
    if (!storage_result.has_value()) {
        return std::unexpected(storage_result.error());
    }
    auto storage = std::move(storage_result.value());
    if (create_index) {
        const auto index_result = storage.save_or_create_index();
        if (!index_result.has_value()) {
            return std::unexpected(store::OpenError<Id>(index_result.error()));
        }
    }
    return storage;
}

template<typename NodeData, typename CodecResolver>
std::expected<store::IndexedStorage<StoreTraits, NodeData>, store::OpenError<Id>>
open_folder_indexed(
    const std::filesystem::path &base_path,
    std::string default_codec_selector,
    CodecResolver resolve_codec,
    OpenOptions options = {}) {
    auto result = open_folder<NodeData>(
        base_path,
        true,
        std::move(default_codec_selector),
        std::move(resolve_codec),
        std::move(options));
    if (!result.has_value()) {
        return std::unexpected(result.error());
    }
    return store::IndexedStorage<StoreTraits, NodeData>(std::move(result.value()));
}

} // namespace octree::storage
