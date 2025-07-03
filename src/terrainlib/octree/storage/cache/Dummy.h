#pragma once

#include <optional>

#include "octree/Id.h"
#include "octree/RawStorage.h"
#include "octree/cache/ICache.h"

namespace octree {

class Dummy : public ICache {
public:
    std::optional<Node> get(const Id &) override { return std::nullopt; }
    bool put(const Id &, const Node &) override { return false; }
    bool remove(const Id &) override { return false; }
    bool contains(const Id &) const override { return false; }
};

}
