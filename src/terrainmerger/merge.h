#pragma once

#include <string>
#include <optional>

#include "mask.h"
#include "merge/NodeData.h"
#include "merge/Result.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "NodeLoader.h"
#include "NodeWriter.h"
#include "octree/storage/cache/Dummy.h"
#include "merge/visitor/Masked.h"
#include "merge/visitor/Simple.h"

inline std::string get_dataset_name(const octree::Storage &storage) {
    return storage.layout().base_path().filename().string();
}

template <auto N>
using smallest_uint_t =
    std::conditional_t<(N <= UINT8_MAX), uint8_t,
                       std::conditional_t<(N <= UINT16_MAX), uint16_t,
                                          std::conditional_t<(N <= UINT32_MAX), uint32_t,
                                                             uint64_t>>>;

template <typename Visitor>
class Merger {
public:
    using Status = octree::NodeStatusOrMissing;

    Merger(
        Visitor& visitor,
        const NodeLoader &left,
        const NodeLoader &right,
        NodeWriter &output) : _visitor(visitor), _left(left), _right(right), _output(output) {

    }

    void merge_node(const octree::Id &id) {
        const Status left_status = this->_left.get_status(id);
        const Status right_status = this->_right.get_status(id);
        return this->merge_node(id, left_status, right_status);
    }

    void merge_node(
        const octree::Id &id,
        const Status left_status,
        const Status right_status
    ) {
        if (this->_output.has_node(id)) {
            return; // Already merged perviously
        }

        const auto merge_result = this->call_merge(id, left_status, right_status);
        std::visit([&](const auto &result) {
            using Result = std::decay_t<decltype(result)>;
            if constexpr (std::is_same_v<Result, merge::Recurse>) {
                DEBUG_ASSERT(id.has_children());
                const auto children = id.children().value();
                for (const auto &child_id : children) {
                    this->merge_node(child_id);
                }
            } else if constexpr (std::is_same_v<Result, merge::Unchanged>) {
                this->_output.copy_subtree_to_output(id, result.is_left ? this->_left : this->_right);
            } else if constexpr (std::is_same_v<Result, merge::Merged>) {
                this->_output.write_node(id, result.mesh);
            } else if constexpr (std::is_same_v<Result, merge::Ignore>) {
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

    merge::Result call_merge(
        const octree::Id &id,
        Status left,
        Status right) {
        const Index index = get_index(left, right);

        switch (index) {
#define ALP_GENERATE_CASE(left_status, right_status)                        \
    case get_index(Status::left_status, Status::right_status): {            \
        merge::NodeData<Status::left_status> left_node(id, this->_left);    \
        merge::NodeData<Status::right_status> right_node(id, this->_right); \
        return this->_visitor.template visit(id, left_node, right_node); \
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

    octree::cache::Dummy left_cache;
    NodeLoader left(left_dataset, left_cache);
    octree::cache::Dummy right_cache;
    NodeLoader right(right_dataset, right_cache);
    NodeWriter output(output_dataset);
    if (mask.has_value()) {
        merge::visitor::Masked visitor {mask.value()};
        Merger<merge::visitor::Masked> merger(visitor, left, right, output);
        merger.merge_node(octree::Id::root());
    } else {
        merge::visitor::Simple visitor;
        Merger<merge::visitor::Simple> merger(visitor, left, right, output);
        merger.merge_node(octree::Id::root());
    }

    output_dataset.save_or_create_index();
}
