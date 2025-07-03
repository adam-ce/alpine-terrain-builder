#pragma once

#include <unordered_map>
#include <optional>

#include <zpp_bits.h>

#include "octree/Id.h"
#include "octree/NodeStatus.h"

namespace octree {

class IndexMap {
public:
    using Container = std::unordered_map<Id, NodeStatus>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    std::optional<NodeStatus> get(Id id) const;
    bool add(Id id);
    bool remove(Id id);

    bool is_present(Id id) const;
    bool is_absent(Id id) const;
    bool is(NodeStatus status, Id id) const;

    void clear();
    bool empty() const;
    size_t size() const;

    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    NodeStatus* get_raw(Id id);
    const NodeStatus* get_raw(Id id) const;
    void set_raw(Id id, NodeStatus status);
    void remove_raw(Id id);

    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;

private:
    Container _index;
    void update_parent_after_remove(Id id);
};

} // namespace octree
