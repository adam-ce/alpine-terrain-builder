#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <expected>

#include "io/envelope.h"
#include "octree/StoreTraits.h"
#include "store/Index.h"

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

using StoreMetadataSchema = io::envelope::PayloadSchema<
    "octree.StoreMetadata",
    io::envelope::Version<1, v1::StoreMetadata>>;
using StoreIndexSchema = io::envelope::PayloadSchema<
    "octree.StoreIndex",
    io::envelope::Version<1, v1::StoreIndex>>;

using StoreMetadata = StoreMetadataSchema::latest_type;
using StoreIndex = StoreIndexSchema::latest_type;

inline std::expected<void, std::string> validate(const StoreMetadata &metadata)
{
    if (metadata.layout_id.empty()) {
        return std::unexpected("layout ID is empty");
    }
    if (metadata.payload_class.empty()) {
        return std::unexpected("payload class is empty");
    }
    if (metadata.codec_selector.empty()) {
        return std::unexpected("codec selector is empty");
    }
    return {};
}

inline StoreIndex encode_index(const store::Index<StoreTraits> &index)
{
    StoreIndex result;
    result.entries.reserve(index.size());
    for (const auto &[id, status] : index) {
        result.entries.push_back({
            .level = id.level(),
            .index = id.index_on_level(),
            .status = static_cast<std::uint8_t>(static_cast<store::NodeStatus::Value>(status)),
        });
    }
    return result;
}

inline std::expected<store::Index<StoreTraits>, std::string> decode_index(
    const StoreIndex &encoded)
{
    store::Index<StoreTraits> result;
    std::unordered_set<Id> seen;
    for (const v1::IndexEntry &entry : encoded.entries) {
        const auto id = Id::try_make(
            static_cast<Id::Level>(entry.level),
            static_cast<Id::Index>(entry.index));
        if (!id) {
            return std::unexpected("index contains an invalid octree ID");
        }
        if (!seen.insert(*id).second) {
            return std::unexpected("index contains a duplicate octree ID");
        }
        if (entry.status > static_cast<std::uint8_t>(store::NodeStatus::Virtual)) {
            return std::unexpected("index contains an invalid node status");
        }
        const auto set = result.set_raw(
            *id,
            store::NodeStatus{static_cast<store::NodeStatus::Value>(entry.status)});
        if (!set) {
            return std::unexpected("index contains an invalid octree ID");
        }
    }

    for (const auto &[id, status] : result) {
        const auto children = StoreTraits::children(id);
        bool has_child = false;
        if (children) {
            for (const Id &child : *children) {
                const auto child_status = result.get(child);
                if (!child_status) {
                    return std::unexpected("index child lookup failed");
                }
                has_child = has_child || child_status->has_value();
            }
        }
        if ((status == store::NodeStatus::Leaf && has_child)
            || ((status == store::NodeStatus::Inner
                 || status == store::NodeStatus::Virtual)
                && !has_child)) {
            return std::unexpected("index contains inconsistent node topology");
        }

        const auto parent = StoreTraits::parent(id);
        if (!parent) {
            if (id != StoreTraits::root()) {
                return std::unexpected("index contains a non-root node without a parent");
            }
            continue;
        }
        const auto parent_status = result.get(*parent);
        if (!parent_status || !parent_status->has_value()
            || parent_status->value() == store::NodeStatus::Leaf) {
            return std::unexpected("index contains a node without a valid indexed parent");
        }
    }
    return result;
}

} // namespace octree::storage
