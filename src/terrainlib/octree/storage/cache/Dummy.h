#pragma once

#include <optional>

#include "octree/Id.h"
#include "octree/storage/RawStorage.h"
#include "octree/storage/cache/ICache.h"

namespace octree::cache {

class Dummy : public ICache {
public:
    std::optional<Node> get(const Id &) noexcept override {
        return std::nullopt;
    }
    bool put(const Id &, const Node &) noexcept override {
        return false;
    }
    bool remove(const Id &) noexcept override {
        return false;
    }
    bool contains(const Id &) const noexcept override {
        return false;
    }
};

}
