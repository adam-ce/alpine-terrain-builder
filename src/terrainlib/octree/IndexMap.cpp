#include <libassert/assert.hpp>

#include "octree/IndexMap.h"
#include "log.h"

namespace octree {

std::optional<NodeStatus> IndexMap::get(Id id) const {
    const NodeStatus* status = this->get_raw(id);
    if (status != nullptr) {
        return *status;
    } else {
        return std::nullopt;
    }
}

bool IndexMap::add(Id id) {
    NodeStatus* status = this->get_raw(id);
    if (status != nullptr) {
        switch (*status) {
            case NodeStatus::Inner:
            case NodeStatus::Leaf:
                // Nothing to do
                return false;
            case NodeStatus::Virtual:
                this->set_raw(id, NodeStatus::Inner);
                return true;
        }
    }

    const auto parent_opt = id.parent();
    if (!parent_opt.has_value()) {
        DEBUG_ASSERT(id.is_root());
        if (this->empty()) {
            this->set_raw(id, NodeStatus::Leaf);
            return true;
        } else {
            UNREACHABLE();
            return true;
        }
    }
    
    const auto parent = parent_opt.value();
    const NodeStatus* parent_status = this->get_raw(parent);
    if (parent_status == nullptr) {
        this->add(parent);
        this->set_raw(parent, NodeStatus::Virtual);
        this->set_raw(id, NodeStatus::Leaf);
        return true;
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

    return true;
}

bool IndexMap::remove(Id id) {
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

bool IndexMap::is_present(Id id) const {
    auto it = this->_index.find(id);
    return it != this->_index.end();
}
bool IndexMap::is_absent(Id id) const {
    return !this->is_present(id);
}
bool IndexMap::is(NodeStatus status, Id id) const {
    return this->get(id) == status;
}

void IndexMap::clear() {
    this->_index.clear();
}
bool IndexMap::empty() const {
    return this->_index.empty();
}
size_t IndexMap::size() const {
    return this->_index.size();
}

IndexMap::const_iterator IndexMap::begin() const {
    return this->_index.begin();
}
IndexMap::const_iterator IndexMap::end() const {
    return this->_index.end();
}
IndexMap::const_iterator IndexMap::cbegin() const {
    return this->_index.cbegin();
}
IndexMap::const_iterator IndexMap::cend() const {
    return this->_index.cend();
}

NodeStatus* IndexMap::get_raw(Id id) {
    if (auto it = this->_index.find(id); it != this->_index.end()) {
        return &it->second;
    }
    return nullptr;
}
const NodeStatus* IndexMap::get_raw(Id id) const {
    if (auto it = this->_index.find(id); it != this->_index.end()) {
        return &it->second;
    }
    return nullptr;
}
void IndexMap::set_raw(Id id, NodeStatus status) {
    this->_index[id] = status;
}
void IndexMap::remove_raw(Id id) {
    this->_index.erase(id);
}

void IndexMap::update_parent_after_remove(Id id) {
    const auto parent_opt = id.parent();
    if (!parent_opt.has_value()) {
        return;
    }
    
    const auto parent = parent_opt.value();
    NodeStatus* parent_status = this->get_raw(parent);
    DEBUG_ASSERT(parent_status != nullptr);

    // We know there are children since we just removed one
    DEBUG_ASSERT(parent.has_children());
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

}
