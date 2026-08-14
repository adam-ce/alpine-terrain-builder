#pragma once

#include <optional>

#include "store/Traits.h"

namespace store::cache {

template<HierarchyTraits Traits, typename NodeData>
class Interface {
public:
    using Key = typename Traits::Key;
    virtual ~Interface() = default;

    virtual std::optional<NodeData> get(const Key &key) noexcept = 0;
    virtual bool put(const Key &key, const NodeData &value) noexcept = 0;
    virtual bool remove(const Key &key) noexcept = 0;
    virtual bool contains(const Key &key) const noexcept = 0;
};

} // namespace store::cache
