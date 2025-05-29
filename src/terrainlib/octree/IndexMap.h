#pragma once

#include <unordered_map>

#include <zpp_bits.h>

#include "octree/Id.h"
#include "octree/NodeStatus.h"
#include "log.h"

namespace octree {

class IndexMap {
public:
    using Container = std::unordered_map<Id, NodeStatus>;
    using iterator = Container::iterator;
    using const_iterator = Container::const_iterator;

    std::optional<NodeStatus> get(Id id) const {
        const NodeStatus* status = this->get_raw(id);
        if (status != nullptr) {
            return *status;
        } else {
            return std::nullopt;
        }
    }

    void add(Id id) {
        NodeStatus* status = this->get_raw(id);
        if (status != nullptr) {
            switch (*status) {
                case NodeStatus::Inner:
                case NodeStatus::Leaf:
                    // Nothing to do
                    break;
                case NodeStatus::Virtual:
                    this->set_raw(id, NodeStatus::Inner);
                    break;
            }
            return;
        }

        const auto parent_opt = id.parent();
        if (!parent_opt.has_value()) {
            assert(id.is_root());
            if (this->empty()) {
                this->set_raw(id, NodeStatus::Leaf);
                return;
            } else {
                UNREACHABLE();
                return;
            }
        }
        
        const auto parent = parent_opt.value();
        const NodeStatus* parent_status = this->get_raw(parent);
        if (parent_status == nullptr) {
            this->add(parent);
            this->set_raw(parent, NodeStatus::Virtual);
            this->set_raw(id, NodeStatus::Leaf);
            return;
        }

        switch (*parent_status) {
            case NodeStatus::Leaf:
                this->set_raw(parent, NodeStatus::Inner);
                this->set_raw(id, NodeStatus::Leaf);
                break;
            case NodeStatus::Inner:
            case NodeStatus::Virtual:
                this->set_raw(id, NodeStatus::Leaf);
                break;
        }
    }

    bool remove(Id id) {
        NodeStatus* status = this->get_raw(id);
        if (status == nullptr) {
            return false;
        }

        switch (*status) {
            case NodeStatus::Inner:
                *status = NodeStatus::Virtual;
                return true;
            case NodeStatus::Virtual:
                // Nothing to do
                return false;
            case NodeStatus::Leaf:
                this->remove_raw(id);
                this->update_parent_after_remove(id);
                return true;
        }

        UNREACHABLE();
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

    
    NodeStatus* get_raw(Id id) {
        if (auto it = this->_index.find(id); it != this->_index.end()) {
            return &it->second;
        }
        return nullptr;
    }
    const NodeStatus* get_raw(Id id) const {
        if (auto it = this->_index.find(id); it != this->_index.end()) {
            return &it->second;
        }
        return nullptr;
    }
    void set_raw(Id id, NodeStatus status) {
        this->_index[id] = status;
    }
    void remove_raw(Id id) {
        this->_index.erase(id);
    }


private:
    Container _index;

    void update_parent_after_remove(Id id) {
        const auto parent_opt = id.parent();
        if (!parent_opt.has_value()) {
            return;
        }
        
        const auto parent = parent_opt.value();
        NodeStatus* parent_status = this->get_raw(parent);
        assert(parent_status != nullptr);

        // We know there are children since we just removed one
        assert(parent.has_children());
        const auto siblings = parent.children().value();
        for (const auto& sibling : siblings) {
            if (this->is_present(sibling) && sibling != id) {
                return;
            }
        }

        switch (*parent_status) {
            case NodeStatus::Inner:
                *parent_status = NodeStatus::Leaf;
                break;
            case NodeStatus::Virtual:
                this->remove_raw(parent);
                this->update_parent_after_remove(parent);
                break;
            case NodeStatus::Leaf:
                UNREACHABLE();
                break;
        }
    }


public:
    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;
};
}
