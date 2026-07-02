#pragma once

#include <optional>

#include "octree/Id.h"
#include "octree/storage/cache/ICache.h"

namespace octree::cache {

 template <typename T>
class Dummy : public ICache<T> {
public:
    std::optional<T> get(const Id &) noexcept override {
        return std::nullopt;
    }
    bool put(const Id &, const T &) noexcept override {
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
