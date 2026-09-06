#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "io/envelope.h"
#include "octree/StoreTraits.h"
#include "octree/store_layout/Mappings.h"
#include "store/Index.h"
#include "store/IndexFormat.h"

namespace octree::storage {

inline constexpr std::string_view metadata_file_name = "octree.storemeta";
inline constexpr std::string_view index_file_name = "octree.storeindex";

namespace v1 {

    struct StoreMetadata {
        std::string layout_id;
        std::string payload_class;
        std::string codec_selector;
    };

    struct IndexEntry {
        std::uint8_t level;
        std::uint64_t index;
        std::uint8_t status;
    };

    struct StoreIndex {
        std::vector<IndexEntry> entries;
    };

} // namespace v1

using StoreMetadataSchema = io::envelope::PayloadSchema<"octree.StoreMetadata", io::envelope::Version<1, v1::StoreMetadata>>;
using StoreIndexSchema = io::envelope::PayloadSchema<"octree.StoreIndex", io::envelope::Version<1, v1::StoreIndex>>;

using StoreMetadata = StoreMetadataSchema::latest_type;
using StoreIndex = StoreIndexSchema::latest_type;

inline Expected<void> validate(const StoreMetadata& metadata)
{
    if (metadata.layout_id.empty()) {
        return Error::fail(Error::Code::InvalidInput, "layout ID is empty");
    }
    if (metadata.payload_class.empty()) {
        return Error::fail(Error::Code::InvalidInput, "payload class is empty");
    }
    if (metadata.codec_selector.empty()) {
        return Error::fail(Error::Code::InvalidInput, "codec selector is empty");
    }
    return {};
}

inline StoreIndex encode_index(const store::Index<StoreTraits>& index)
{
    StoreIndex result;
    result.entries.reserve(index.size());
    for (const auto& [id, status] : index) {
        result.entries.push_back({
            .level = id.level(),
            .index = id.index_on_level(),
            .status = static_cast<std::uint8_t>(static_cast<store::NodeStatus::Value>(status)),
        });
    }
    return result;
}

inline Expected<store::Index<StoreTraits>> decode_index(const StoreIndex& encoded)
{
    store::Index<StoreTraits> result;
    std::unordered_set<Id> seen;
    for (const v1::IndexEntry& entry : encoded.entries) {
        const auto id = Id::try_make(static_cast<Id::Level>(entry.level), static_cast<Id::Index>(entry.index));
        if (!id) {
            return Error::fail(Error::Code::CorruptData, "index contains an invalid octree ID");
        }
        if (!seen.insert(*id).second) {
            return Error::fail(Error::Code::CorruptData, "index contains a duplicate octree ID");
        }
        if (entry.status > static_cast<std::uint8_t>(store::NodeStatus::Virtual)) {
            return Error::fail(Error::Code::CorruptData, "index contains an invalid node status");
        }
        auto set = result.set_raw(*id, store::NodeStatus { static_cast<store::NodeStatus::Value>(entry.status) });
        if (!set) {
            return Error::propagate(std::move(set), Error::Code::CorruptData, "add decoded node to store index");
        }
    }

    for (const auto& [id, status] : result) {
        const auto children = StoreTraits::children(id);
        bool has_child = false;
        if (children) {
            for (const Id& child : *children) {
                auto child_status = result.get(child);
                if (!child_status) {
                    return Error::propagate(std::move(child_status), Error::Code::CorruptData, "look up child while validating store index");
                }
                has_child = has_child || child_status->has_value();
            }
        }
        if ((status == store::NodeStatus::Leaf && has_child) || ((status == store::NodeStatus::Inner || status == store::NodeStatus::Virtual) && !has_child)) {
            return Error::fail(Error::Code::CorruptData, "index contains inconsistent node topology");
        }

        const auto parent = StoreTraits::parent(id);
        if (!parent) {
            if (id != StoreTraits::root()) {
                return Error::fail(Error::Code::CorruptData, "index contains a non-root node without a parent");
            }
            continue;
        }
        auto parent_status = result.get(*parent);
        if (!parent_status) {
            return Error::propagate(std::move(parent_status), Error::Code::CorruptData, "look up parent while validating store index");
        }
        if (!parent_status->has_value() || parent_status->value() == store::NodeStatus::Leaf) {
            return Error::fail(Error::Code::CorruptData, "index contains a node without a valid indexed parent");
        }
    }
    return result;
}

inline Expected<StoreMetadata> read_store_metadata(const std::filesystem::path& base_path)
{
    const std::filesystem::path path = base_path / metadata_file_name;
    auto metadata = io::envelope::read_from_path<StoreMetadataSchema>(path);
    if (!metadata) {
        return metadata;
    }
    if (auto valid = validate(*metadata); !valid) {
        return Error::propagate(std::move(valid), Error::Code::CorruptData, "validate store metadata read from \"" + path.string() + "\"");
    }
    return std::move(*metadata);
}

inline Expected<store::IndexMetadata<StoreTraits>> read_index_file(const std::filesystem::path& path)
{
    auto metadata = read_store_metadata(path.parent_path());
    if (!metadata) {
        return Error::propagate(std::move(metadata), "read octree store metadata");
    }

    auto encoded_index = io::envelope::read_from_path<StoreIndexSchema>(path);
    if (!encoded_index) {
        return Error::propagate(std::move(encoded_index), "read octree store index \"" + path.string() + "\"");
    }
    auto index = decode_index(*encoded_index);
    if (!index) {
        return Error::propagate(std::move(index), "decode store index read from \"" + path.string() + "\"");
    }
    return store::IndexMetadata<StoreTraits> {
        std::move(*index),
        std::move(metadata->layout_id),
        std::move(metadata->payload_class),
        std::move(metadata->codec_selector),
    };
}

inline Expected<void> write_index_file(const std::filesystem::path& path, const store::IndexMetadata<StoreTraits>& metadata)
{
    const StoreMetadata store_metadata {
        .layout_id = metadata.layout_id,
        .payload_class = metadata.payload_class,
        .codec_selector = metadata.codec_selector,
    };
    if (auto valid = validate(store_metadata); !valid) {
        return Error::propagate(std::move(valid), "validate store metadata for writing");
    }
    const std::filesystem::path metadata_path = path.parent_path() / metadata_file_name;
    if (auto written = io::envelope::write_to_path<StoreMetadataSchema>(store_metadata, metadata_path); !written) {
        return Error::propagate(std::move(written), "write octree store metadata " + metadata_path.string());
    }

    const StoreIndex encoded_index = encode_index(metadata.index);
    if (auto written = io::envelope::write_to_path<StoreIndexSchema>(encoded_index, path); !written) {
        return Error::propagate(std::move(written), "write octree store index " + path.string());
    }
    return {};
}

inline store::path_layout::Mapping<Id> default_mapping() { return store_layout::level_and_coordinate_directories(); }

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
