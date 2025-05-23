#pragma once

#include <unordered_map>

#include <zpp_bits.h>

#include "octree/Id.h"
#include "octree/NodeStatus.h"

namespace octree {

class IndexMap {
public:
    using Container = std::unordered_map<Id, NodeStatus>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    std::optional<NodeStatus> get(Id id) const {
        if (auto it = this->_index.find(id); it != this->_index.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void set(Id id, NodeStatus status) {
        // TODO: update other nodes?
        this->_index[id] = status;
    }

    bool is_present(Id id) const {
        auto it = this->_index.find(id);
        return it != this->_index.end();
    }
    bool is_absent(Id id) const {
        return !this->is_present(id);
    }
    bool is(NodeStatus status, Id id) const {
        return this->get(id) == status;
    }

    void clear() {
        this->_index.clear();
    }
    bool empty() const {
        return this->_index.empty();
    }
    size_t size() const {
        return this->_index.size();
    }

    const_iterator begin() const {
        return this->_index.begin();
    }
    const_iterator end() const {
        return this->_index.end();
    }
    const_iterator cbegin() const {
        return this->_index.cbegin();
    }
    const_iterator cend() const {
        return this->_index.cend();
    }

private:
    Container _index;
public:
    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;
};
}
