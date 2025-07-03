#pragma once

#include <filesystem>

#include <tl/expected.hpp>

#include "mesh/SimpleMesh.h"
#include "octree/Id.h"
#include "mesh/io/error.h"

namespace octree {
using Node = SimpleMesh;

struct IdAndNode {
    Id id;
    Node node;

    IdAndNode() = default;
    IdAndNode(Id id, Node node) : id(std::move(id)), node(std::move(node)) {}
    
    operator Node() const {
        return std::move(node);
    }
    operator Id() const {
        return id;
    }
};

class IStorage {
public:
    virtual ~IStorage() = default;
    virtual tl::expected<Node, mesh::io::LoadMeshError> read_node(const Id &id) const = 0;
    virtual tl::expected<void, mesh::io::SaveMeshError> write_node(const Id &id, const Node &node) = 0;
    virtual bool remove_node(const Id &id) = 0;
    virtual bool has_node(const Id &id) const = 0;
    virtual std::filesystem::path get_node_path(const Id &id) const = 0;
    virtual std::filesystem::path base_path() const = 0;
};

} // namespace octree
