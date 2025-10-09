#pragma once

#include <span>
#include <functional>

#include "mesh/SimpleMesh.h"
#include "atlas/Layout.h"

namespace atlas {

class LayoutPlanner {
public:
    virtual ~LayoutPlanner() = default;
    virtual Layout plan(
        const std::span<const std::reference_wrapper<const SimpleMesh>> meshes) const = 0;
};

} // namespace atlas
