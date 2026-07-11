#pragma once

#include <optional>

#include "octree/Id.h"

namespace octree::cache {

template <typename T>
class ICache {
public:
    virtual ~ICache() = default;

    virtual std::optional<T> get(const Id& id) noexcept = 0;
    virtual bool put(const Id &id, const T &value) noexcept = 0;
    virtual bool remove(const Id &id) noexcept = 0;
    virtual bool contains(const Id &id) const noexcept = 0;
};
 
}
