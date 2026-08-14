#pragma once

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
#include "octree/NodeStatusOrMissing.h"
#include "octree/storage/cache/Dummy.h"
#include "store/describe_error.h"

inline std::string get_dataset_name(const octree::Storage &storage) {
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
    using Status = octree::NodeStatusOrMissing;
    using Context = Visitor::Context;
    using Result = merge::Result<Context>;

    Merger(
        Visitor& visitor,
        NodeLoader left,
        NodeLoader right,
        NodeWriter output) : _visitor(visitor), _left(left), _right(right), _output(output) {
    }

    void merge_root() {
        const octree::Id id = octree::Id::root();
        const Context ctx = this->_visitor.make_root_context();
        merge_node(id, ctx);
    }

    void merge_node(const octree::Id &id, const Context& ctx) {
        const Status left_status = this->_left.get_status(id);
        const Status right_status = this->_right.get_status(id);
        return this->merge_node(id, left_status, right_status, ctx);
    }

    void merge_node(
        const octree::Id &id,
        const Status left_status,
        const Status right_status,
        const Context& ctx
    ) {
        LOG_DEBUG("[{}] Start merging (left = {}, right = {})", id, left_status, right_status);
        if (this->_output.has_node(id)) {
            LOG_DEBUG("[{}] Already merged, skipping...", id);
            return; // Already merged previously
        }

        const auto merge_result = this->call_merge(id, left_status, right_status, ctx);
        std::visit([&](const auto &result) {
            using Result = std::decay_t<decltype(result)>;
            if constexpr (std::is_same_v<Result, merge::Recurse<Context>>) {
                LOG_DEBUG("[{}] needs recursion", id);
                DEBUG_ASSERT(id.has_children());
                const auto children = id.children().value();
                for (const auto &child_id : children) {
                    this->merge_node(child_id, result.context);
                }
            } else if constexpr (std::is_same_v<Result, merge::Unchanged>) {
                LOG_DEBUG("[{}] remains unchanged (same as {})", id, (result.source == merge::Source::Left ? "left" : "right"));
                // If the node should remain unchanged, but its not present on disk we cannot copy it.
                if (result.source == merge::Source::Left && left_status == Status::Missing) {
                    auto mesh_opt = this->_left.load_node(id);
                    if (mesh_opt.has_value()) {
                        this->_output.write_node(id, mesh_opt.value());
                    }
                } else if (result.source == merge::Source::Right && right_status == Status::Missing) {
                    auto mesh_opt = this->_right.load_node(id);
                    if (mesh_opt.has_value()) {
                        this->_output.write_node(id, mesh_opt.value());
                    }
                } else {
                    this->_output.copy_subtree_to_output(id, result.source == merge::Source::Left ? this->_left : this->_right);
                }
            } else if constexpr (std::is_same_v<Result, merge::Merged>) {
                LOG_DEBUG("[{}] was merged", id);
                this->_output.write_node(id, result.mesh);
            } else if constexpr (std::is_same_v<Result, merge::Ignore>) {
                LOG_DEBUG("[{}] was ignored", id);
                // do nothing
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

inline void merge_datasets(
    const octree::IndexedStorage &left_dataset,
    const octree::IndexedStorage &right_dataset,
    octree::Storage &output_dataset,
    const std::optional<MeshMask> mask = std::nullopt) {
    LOG_TRACE("Merging {} and {} into {}",
        get_dataset_name(left_dataset),
        get_dataset_name(right_dataset),
        get_dataset_name(output_dataset));

    octree::Space space = octree::Space::earth();
    octree::cache::Dummy<mesh::Simple> left_cache;
    NodeLoader left(left_dataset, left_cache, space);
    octree::cache::Dummy<mesh::Simple> right_cache;
    NodeLoader right(right_dataset, right_cache, space);
    NodeWriter output(output_dataset);
    if (mask.has_value()) {
        merge::visitor::Masked visitor {mask.value(), space};
        Merger<merge::visitor::Masked> merger(visitor, left, right, output);
        merger.merge_root();
    } else {
        merge::visitor::Simple visitor;
        Merger<merge::visitor::Simple> merger(visitor, left, right, output);
        merger.merge_root();
    }

    const auto index_result = output_dataset.save_or_create_index();
    if (!index_result.has_value()) {
        LOG_ERROR_AND_EXIT(
            "Failed to save output index in {}: {}",
            output_dataset.base_path(),
            store::describe_error(index_result.error()));
    }
}
