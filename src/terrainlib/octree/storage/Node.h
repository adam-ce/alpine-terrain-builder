#pragma once

#include "mesh/SimpleMesh.h"
#include "octree/Id.h"

namespace octree {
using Node = SimpleMesh;

struct IdAndNode {
    Id id;
    Node node;

    IdAndNode() = default;
    IdAndNode(Id id, Node node) : id(std::move(id)), node(std::move(node)) {}
    
    operator Node() const { return node; }
    operator Node() && { return std::move(node); }
    operator Id() const { return id; }
};

} // namespace octree
