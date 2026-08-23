#pragma once

#include "octree/Id.h"

namespace sf {

struct InvalidTopology {
    octree::Id key;

    bool operator==(const InvalidTopology&) const = default;
};

} // namespace sf
