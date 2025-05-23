#pragma once

#include <memory>

#include "octree/disk/layout/strategy/LevelAndCoordinateDirectories.h"

namespace octree::disk::layout::strategy {

using Default = LevelAndCoordinateDirectories;

inline std::unique_ptr<Strategy> make_default() {
    return std::make_unique<Default>();
}

} // namespace octree::disk::layout::strategy
