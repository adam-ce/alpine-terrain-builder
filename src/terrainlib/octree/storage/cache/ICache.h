#pragma once

#include <optional>

#include "octree/Id.h"
#include "octree/storage/IStorage.h"

namespace octree {
   
class ICache {
public:
    virtual std::optional<Node> get(const Id& id) noexcept = 0;
    virtual bool put(const Id &id, const Node &node) noexcept = 0;
    virtual bool remove(const Id &id) noexcept = 0;
    virtual bool contains(const Id &id) const noexcept = 0;
};
 
}
