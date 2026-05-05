#pragma once

#include <optional>

#include "octree/Id.h"
#include "octree/storage/Node.h"

namespace octree::cache {
   
class ICache {
public:
    virtual ~ICache() = default;

    virtual std::optional<Node> get(const Id& id) noexcept = 0;
    virtual bool put(const Id &id, const Node &node) noexcept = 0;
    virtual bool remove(const Id &id) noexcept = 0;
    virtual bool contains(const Id &id) const noexcept = 0;
};
 
}
