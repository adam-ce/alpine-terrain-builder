/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2024 Martin Braunsperger
 * Copyright (C) 2024 Adam Celarek-Litofcenko
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

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "Error.h"
#include "NodeLoader.h"
#include "NodeWriter.h"
#include "inputs.h"
#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "merge/visitor/Masked.h"
#include "merge/visitor/Simple.h"
#include "merge/visitor/Visitor.h"
#include "mesh/storage.h"
#include "octree/Id.h"
#include "sf/finalize_storage.h"
#include "sf/validate_index.h"
#include "store/NodeStatusOrMissing.h"
#include "store/cache/Dummy.h"
#include "store/inputs.h"
#include "store/snapshot.h"

inline std::string get_dataset_name(const mesh::storage::Storage &storage) {
    return storage.layout().base_path().filename().string();
}

template <auto N>
using smallest_uint_t =
    std::conditional_t<(N <= UINT8_MAX), uint8_t,
                       std::conditional_t<(N <= UINT16_MAX), uint16_t,
                                          std::conditional_t<(N <= UINT32_MAX), uint32_t,
                                                             uint64_t>>>;

template <merge::Visitor Visitor>
class Merger {
public:
    using Status = store::NodeStatusOrMissing;
    using Context = Visitor::Context;
    using Result = merge::Result<Context>;

    Merger(Visitor& visitor,
        NodeLoader left,
        NodeLoader right,
        NodeWriter output,
        const mesh::storage::IndexedStorage* cache = nullptr,
        std::function<bool()> stop_requested = {})
        : _visitor(visitor)
        , _left(left)
        , _right(right)
        , _output(output)
        , _cache(cache)
        , _stop_requested(std::move(stop_requested))
    {
    }

    Expected<void> merge_root() {
        const octree::Id id = octree::Id::root();
        const Context ctx = this->_visitor.make_root_context();
        return merge_node(id, ctx);
    }

    Expected<void> merge_node(const octree::Id &id, const Context& ctx) {
        const Status left_status = this->_left.get_status(id);
        const Status right_status = this->_right.get_status(id);
        return this->merge_node(id, left_status, right_status, ctx);
    }

    Expected<void> merge_node(
        const octree::Id &id,
        const Status left_status,
        const Status right_status,
        const Context& ctx
    ) {
        LOG_DEBUG("[{}] Start merging (left = {}, right = {})", id, left_status, right_status);
        if (auto polled = this->poll(); !polled) {
            return polled;
        }
        if (this->_cache != nullptr) {
            auto cached = this->_cache->has(id);
            if (!cached) {
                return Error::propagate(std::move(cached), "check merge cache for node " + id.to_string());
            }
            if (cached.value()) {
                LOG_DEBUG("[{}] Restored from cache", id);
                return this->_output.link_node(id, *this->_cache);
            }
        }

        const auto merge_result = this->call_merge(id, left_status, right_status, ctx);
        return std::visit([&](const auto &result) -> Expected<void> {
            using Result = std::decay_t<decltype(result)>;
            if constexpr (std::is_same_v<Result, merge::Recurse<Context>>) {
                LOG_DEBUG("[{}] needs recursion", id);
                DEBUG_ASSERT(id.has_children());
                const auto children = id.children().value();
                for (const auto &child_id : children) {
                    auto child_result = this->merge_node(child_id, result.context);
                    if (!child_result) {
                        return child_result;
                    }
                }
                return {};
            } else if constexpr (std::is_same_v<Result, merge::Unchanged>) {
                LOG_DEBUG("[{}] remains unchanged (same as {})", id, (result.source == merge::Source::Left ? "left" : "right"));
                // If the node should remain unchanged, but its not present on disk we cannot link it.
                if (result.source == merge::Source::Left && left_status == Status::Missing) {
                    auto mesh_opt = this->_left.load_node(id);
                    if (mesh_opt.has_value()) {
                        auto write_result = this->_output.write_node(id, *mesh_opt);
                        if (!write_result) {
                            return write_result;
                        }
                    }
                } else if (result.source == merge::Source::Right && right_status == Status::Missing) {
                    auto mesh_opt = this->_right.load_node(id);
                    if (mesh_opt.has_value()) {
                        auto write_result = this->_output.write_node(id, *mesh_opt);
                        if (!write_result) {
                            return write_result;
                        }
                    }
                } else {
                    auto link_result = this->_output.link_subtree_to_output(
                        id,
                        result.source == merge::Source::Left ? this->_left : this->_right,
                        [this] { return this->poll(); });
                    if (!link_result) {
                        return link_result;
                    }
                }
                return {};
            } else if constexpr (std::is_same_v<Result, merge::Merged>) {
                LOG_DEBUG("[{}] was merged", id);
                auto write_result = this->_output.write_node(id, result.mesh);
                if (!write_result) {
                    return write_result;
                }
                return {};
            } else if constexpr (std::is_same_v<Result, merge::Ignore>) {
                LOG_DEBUG("[{}] was ignored", id);
                return {};
            }
        }, merge_result);
    }

private:
    // Stops before the next node when requested, and checkpoints the output index.
    Expected<void> poll()
    {
        if (this->_stop_requested && this->_stop_requested()) {
            return Error::fail(Error::Code::Cancelled, "SF merge cancelled; incomplete snapshot retained");
        }
        if (std::chrono::steady_clock::now() - this->_last_checkpoint < store::snapshot::checkpoint_interval) {
            return {};
        }
        this->_last_checkpoint = std::chrono::steady_clock::now();
        return this->_output.save_index();
    }

    static constexpr auto STATUSES = magic_enum::enum_values<Status::Value>();
    static constexpr auto _STATUS_COUNT = STATUSES.size();
    static constexpr auto _MAX_INDEX = _STATUS_COUNT * _STATUS_COUNT;
    using Index = smallest_uint_t<_MAX_INDEX>;
    static constexpr Index STATUS_COUNT = static_cast<Index>(_STATUS_COUNT);
    static constexpr Index MAX_INDEX = static_cast<Index>(_MAX_INDEX);

    static constexpr Index get_index(Status left, Status right) {
        return static_cast<Index>(left) * STATUS_COUNT + static_cast<Index>(right);
    }

    Result call_merge(
        const octree::Id &id,
        Status left,
        Status right,
        const Context& ctx) {
        const Index index = get_index(left, right);

        switch (index) {
#define ALP_GENERATE_CASE(left_status, right_status)                        \
    case get_index(Status::left_status, Status::right_status): {            \
        merge::NodeData<Status::left_status> left_node(id, this->_left);    \
        merge::NodeData<Status::right_status> right_node(id, this->_right); \
        return this->_visitor.template visit<Status::left_status, Status::right_status>(id, left_node, right_node, ctx); \
    }

            ALP_GENERATE_CASE(Missing, Missing);
            ALP_GENERATE_CASE(Missing, Leaf);
            ALP_GENERATE_CASE(Missing, Virtual);

            ALP_GENERATE_CASE(Leaf, Missing);
            ALP_GENERATE_CASE(Leaf, Leaf);
            ALP_GENERATE_CASE(Leaf, Virtual);

            ALP_GENERATE_CASE(Virtual, Missing);
            ALP_GENERATE_CASE(Virtual, Leaf);
            ALP_GENERATE_CASE(Virtual, Virtual);
#undef ALP_GENERATE_CASE

        default:
            UNREACHABLE();
        }
    }

    Visitor& _visitor;
    NodeLoader _left;
    NodeLoader _right;
    NodeWriter _output;
    const mesh::storage::IndexedStorage* _cache;
    std::function<bool()> _stop_requested;
    std::chrono::steady_clock::time_point _last_checkpoint = std::chrono::steady_clock::now();
};

// Merges into output_dataset, linking nodes that the cache has instead of merging them.
// When stop_requested returns true, the merge stops before the next node with an error
// with code Cancelled. On failure, the output index is saved for a later cache.
inline Expected<void> merge_datasets(const mesh::storage::IndexedStorage& left_dataset,
    const mesh::storage::IndexedStorage& right_dataset,
    mesh::storage::Storage& output_dataset,
    const std::optional<MeshMask> mask = std::nullopt,
    const mesh::storage::IndexedStorage* cache = nullptr,
    const std::function<bool()>& stop_requested = {})
{
    for (const mesh::storage::IndexedStorage *input : {&left_dataset, &right_dataset}) {
        auto validation = sf::validate_index(input->index());
        if (!validation) {
            return Error::propagate(
                std::move(validation), "validate merge input dataset \"" + get_dataset_name(*input) + "\"");
        }
    }

    LOG_TRACE("Merging {} and {} into {}",
        get_dataset_name(left_dataset),
        get_dataset_name(right_dataset),
        get_dataset_name(output_dataset));

    octree::Space space = octree::Space::earth();
    store::cache::Dummy<octree::StoreTraits, mesh::Simple> left_cache;
    NodeLoader left(left_dataset, left_cache, space);
    store::cache::Dummy<octree::StoreTraits, mesh::Simple> right_cache;
    NodeLoader right(right_dataset, right_cache, space);
    NodeWriter output(output_dataset);
    Expected<void> result;
    if (mask.has_value()) {
        merge::visitor::Masked visitor {mask.value(), space};
        Merger<merge::visitor::Masked> merger(visitor, left, right, output, cache, stop_requested);
        result = merger.merge_root();
    } else {
        merge::visitor::Simple visitor;
        Merger<merge::visitor::Simple> merger(visitor, left, right, output, cache, stop_requested);
        result = merger.merge_root();
    }
    if (!result) {
        if (auto saved = output_dataset.save_or_create_index(); !saved) {
            LOG_ERROR("Failed to save the index of the incomplete merge output: {}", saved.error().to_string());
        }
        if (result.error().code() == Error::Code::Cancelled) {
            return result;
        }
        return Error::propagate(std::move(result), mask.has_value() ? "merge masked datasets" : "merge datasets");
    }

    auto finalized = sf::finalize_storage(output_dataset);
    if (!finalized) {
        return Error::propagate(
            std::move(finalized), "finalize merged output dataset \"" + get_dataset_name(output_dataset) + "\"");
    }
    return {};
}

// Merges into a new attempt and publishes it to output_path; see store::snapshot.
// The cache must be an incomplete merge output with the same record.
inline Expected<void> merge_datasets(const mesh::storage::IndexedStorage& left_dataset,
    const mesh::storage::IndexedStorage& right_dataset,
    const std::filesystem::path& output_path,
    const std::optional<MeshMask>& mask,
    const sf_merger::inputs::Record& record,
    const std::optional<std::filesystem::path>& cache_path = std::nullopt,
    const std::function<bool()>& stop_requested = {})
{
    for (const mesh::storage::IndexedStorage* input : { &left_dataset, &right_dataset }) {
        if (auto linkable = store::snapshot::check_link_filesystem(input->base_path(), output_path); !linkable) {
            return linkable;
        }
    }
    std::optional<mesh::storage::IndexedStorage> cache;
    if (cache_path.has_value()) {
        if (auto valid = store::inputs::validate_cache<sf_merger::inputs::Schema>(*cache_path, record); !valid) {
            return Error::propagate(std::move(valid), "validate SF merge cache");
        }
        if (auto linkable = store::snapshot::check_link_filesystem(*cache_path, output_path); !linkable) {
            return linkable;
        }
        auto opened = mesh::storage::open_index(*cache_path / octree::storage::index_file_name);
        if (!opened) {
            return Error::propagate(std::move(opened), "open SF merge cache");
        }
        cache.emplace(std::move(*opened));
    }

    mesh::storage::OpenOptions options;
    options.default_mapping = left_dataset.layout().mapping();
    options.preferred_extension = std::string(left_dataset.codec_selector().value_or(".sfmesh"));
    auto output_result = mesh::storage::create(output_path, std::move(options));
    if (!output_result) {
        return Error::propagate(std::move(output_result), "create merge output dataset \"" + output_path.string() + "\"");
    }
    mesh::storage::IndexedStorage output = std::move(output_result.value());
    if (auto written = store::inputs::write<sf_merger::inputs::Schema>(output.base_path(), record); !written) {
        return written;
    }

    auto merged = merge_datasets(left_dataset, right_dataset, output, mask, cache ? &cache.value() : nullptr, stop_requested);
    if (!merged) {
        return merged;
    }
    auto published = mesh::storage::publish(std::move(output));
    if (!published) {
        return Error::propagate(std::move(published), "publish merge output dataset \"" + output_path.string() + "\"");
    }
    return {};
}
