#pragma once

#include <cstdint>

#include "octree/Id.h"
#include "hash_utils.h"

namespace dag {

// A reference to a cluster within a clustered octree node.
struct Id {
    octree::Id source_batch;
    uint32_t cluster_index;

    auto operator<=>(const Id &) const = default;
};

}

template <>
struct std::hash<dag::Id> {
    size_t operator()(const dag::Id &id) const  {
        return ::hash::combine(id.source_batch, id.cluster_index);
    }
};

template <>
struct fmt::formatter<dag::Id> {
    constexpr auto parse(fmt::format_parse_context &ctx) { return ctx.begin(); }
    auto format(const dag::Id &id, fmt::format_context &ctx) const {
        return fmt::format_to(ctx.out(), "{}:{}", id.source_batch, id.cluster_index);
    }
};
