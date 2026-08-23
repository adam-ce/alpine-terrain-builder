#pragma once

#include <utility>

#include "store/Storage.h"

namespace store {

template <HierarchyTraits Traits, typename NodeData>
class IndexedStorage : public Storage<Traits, NodeData> {
public:
    using Base = Storage<Traits, NodeData>;
    using typename Base::Persistence;

    explicit IndexedStorage(Storage<Traits, NodeData> storage)
        : Base(std::move(storage))
    {
        this->ensure_indexed();
    }

    IndexedStorage(RawStorage<Traits, NodeData> raw, Index<Traits> index, Persistence persistence)
        : Base(std::move(raw), std::move(index), std::move(persistence))
    {
    }

    IndexedStorage(const IndexedStorage&) = delete;
    IndexedStorage& operator=(const IndexedStorage&) = delete;
    IndexedStorage(IndexedStorage&&) noexcept = default;
    IndexedStorage& operator=(IndexedStorage&&) noexcept = default;

    const Index<Traits>& index() const { return this->index_ref(); }
};

} // namespace store
