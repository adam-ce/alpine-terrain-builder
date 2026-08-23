#pragma once

#include "store/cache/Interface.h"

namespace store::cache {

template <HierarchyTraits Traits, typename NodeData>
class Dummy final : public Interface<Traits, NodeData> {
public:
    using Key = typename Traits::Key;
    std::optional<NodeData> get(const Key&) noexcept override { return std::nullopt; }
    bool put(const Key&, const NodeData&) noexcept override { return false; }
    bool remove(const Key&) noexcept override { return false; }
    bool contains(const Key&) const noexcept override { return false; }
};

} // namespace store::cache
