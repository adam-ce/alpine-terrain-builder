#pragma once

#include <expected>
#include <string>
#include <optional>

#include "NodeLoader.h"
#include "NodeWriter.h"
#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "merge/visitor/Masked.h"
#include "merge/visitor/Simple.h"
#include "merge/visitor/Visitor.h"
#include "octree/Id.h"
#include "mesh/storage.h"
#include "store/NodeStatusOrMissing.h"
#include "store/cache/Dummy.h"
#include "Error.h"
#include "sf/finalize_storage.h"
#include "sf/validate_index.h"

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

    Merger(
        Visitor& visitor,
        NodeLoader left,
        NodeLoader right,
        NodeWriter output) : _visitor(visitor), _left(left), _right(right), _output(output) {
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
        auto has_result = this->_output.has_node(id);
        if (!has_result.has_value()) {
            return Error::propagate(std::move(has_result));
        }
        if (has_result.value()) {
            LOG_DEBUG("[{}] Already merged, skipping...", id);
            return {}; // Already merged previously
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
                    if (!child_result.has_value()) {
                        return child_result;
                    }
                }
                return {};
            } else if constexpr (std::is_same_v<Result, merge::Unchanged>) {
                LOG_DEBUG("[{}] remains unchanged (same as {})", id, (result.source == merge::Source::Left ? "left" : "right"));
                // If the node should remain unchanged, but its not present on disk we cannot copy it.
                if (result.source == merge::Source::Left && left_status == Status::Missing) {
                    auto mesh_opt = this->_left.load_node(id);
                    if (mesh_opt.has_value()) {
                        auto write_result = this->_output.write_node(id, *mesh_opt);
                        if (!write_result.has_value()) {
                            return write_result;
                        }
                    }
                } else if (result.source == merge::Source::Right && right_status == Status::Missing) {
                    auto mesh_opt = this->_right.load_node(id);
                    if (mesh_opt.has_value()) {
                        auto write_result = this->_output.write_node(id, *mesh_opt);
                        if (!write_result.has_value()) {
                            return write_result;
                        }
                    }
                } else {
                    auto copy_result = this->_output.copy_subtree_to_output(
                        id,
                        result.source == merge::Source::Left ? this->_left : this->_right);
                    if (!copy_result.has_value()) {
                        return copy_result;
                    }
                }
                return {};
            } else if constexpr (std::is_same_v<Result, merge::Merged>) {
                LOG_DEBUG("[{}] was merged", id);
                auto write_result = this->_output.write_node(id, result.mesh);
                if (!write_result.has_value()) {
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
};

inline Expected<void> merge_datasets(
    const mesh::storage::IndexedStorage &left_dataset,
    const mesh::storage::IndexedStorage &right_dataset,
    mesh::storage::Storage &output_dataset,
    const std::optional<MeshMask> mask = std::nullopt) {
    for (const mesh::storage::IndexedStorage *input : {&left_dataset, &right_dataset}) {
        auto validation = sf::validate_index(input->index());
        if (!validation.has_value()) {
            return validation;
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
    if (mask.has_value()) {
        merge::visitor::Masked visitor {mask.value(), space};
        Merger<merge::visitor::Masked> merger(visitor, left, right, output);
        auto result = merger.merge_root();
        if (!result.has_value()) {
            return result;
        }
    } else {
        merge::visitor::Simple visitor;
        Merger<merge::visitor::Simple> merger(visitor, left, right, output);
        auto result = merger.merge_root();
        if (!result.has_value()) {
            return result;
        }
    }

    return sf::finalize_storage(output_dataset);
}
