#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <unordered_map>
#include <utility>

#include "Error.h"
#include "log.h"
#include "store/NodeStatus.h"
#include "store/Traits.h"
#include <libassert/assert.hpp>

namespace store {

template <HierarchyTraits Traits>
class Index {
public:
    using Key = typename Traits::Key;
    using Container = std::unordered_map<Key, NodeStatus, typename Traits::Hasher>;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    Expected<std::optional<NodeStatus>> get(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        const NodeStatus* status = get_raw_unchecked(key);
        if (status == nullptr) {
            return std::nullopt;
        }
        return *status;
    }

    Expected<bool> add(const Key& key)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }

        NodeStatus* status = get_raw_unchecked(key);
        if (status != nullptr) {
            switch (*status) {
            case NodeStatus::Inner:
            case NodeStatus::Leaf:
                return false;
            case NodeStatus::Virtual:
                set_raw_unchecked(key, NodeStatus::Inner);
                return true;
            }
        }

        const auto parent = Traits::parent(key);
        if (!parent.has_value()) {
            DEBUG_ASSERT(key == Traits::root());
            if (empty()) {
                set_raw_unchecked(key, NodeStatus::Leaf);
                return true;
            }
            UNREACHABLE();
        }

        if (!Traits::is_valid(parent.value())) {
            return store::invalid_key_error<Traits>(parent.value());
        }
        NodeStatus* parent_status = get_raw_unchecked(parent.value());
        if (parent_status == nullptr) {
            auto parent_result = add(parent.value());
            if (!parent_result) {
                return parent_result;
            }
            set_raw_unchecked(parent.value(), NodeStatus::Virtual);
            set_raw_unchecked(key, NodeStatus::Leaf);
            return true;
        }

        switch (*parent_status) {
        case NodeStatus::Leaf:
            set_raw_unchecked(parent.value(), NodeStatus::Inner);
            set_raw_unchecked(key, NodeStatus::Leaf);
            break;
        case NodeStatus::Inner:
        case NodeStatus::Virtual:
            set_raw_unchecked(key, NodeStatus::Leaf);
            break;
        }
        return true;
    }

    Expected<bool> remove(const Key& key)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }

        NodeStatus* status = get_raw_unchecked(key);
        if (status == nullptr) {
            return false;
        }
        switch (*status) {
        case NodeStatus::Inner:
            *status = NodeStatus::Virtual;
            return true;
        case NodeStatus::Virtual:
            return false;
        case NodeStatus::Leaf:
            remove_raw_unchecked(key);
            return update_parent_after_remove(key);
        }
        UNREACHABLE();
    }

    Expected<bool> is_present(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return m_index.contains(key);
    }
    Expected<bool> is_absent(const Key& key) const
    {
        auto present = is_present(key);
        if (!present) {
            return present;
        }
        return !present.value();
    }
    Expected<bool> is(const NodeStatus status, const Key& key) const
    {
        auto result = get(key);
        if (!result) {
            return Error::propagate(std::move(result));
        }
        return result.value() == status;
    }

    Expected<NodeStatus*> get_raw(const Key& key)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return get_raw_unchecked(key);
    }
    Expected<const NodeStatus*> get_raw(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        return get_raw_unchecked(key);
    }
    Expected<void> set_raw(const Key& key, const NodeStatus status)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        set_raw_unchecked(key, status);
        return {};
    }
    Expected<void> remove_raw(const Key& key)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        remove_raw_unchecked(key);
        return {};
    }

    void clear() { m_index.clear(); }
    bool empty() const { return m_index.empty(); }
    size_t size() const { return m_index.size(); }

    const_iterator begin() const { return m_index.begin(); }
    const_iterator end() const { return m_index.end(); }
    const_iterator cbegin() const { return m_index.cbegin(); }
    const_iterator cend() const { return m_index.cend(); }

private:
    NodeStatus* get_raw_unchecked(const Key& key)
    {
        const auto iterator = m_index.find(key);
        return iterator == m_index.end() ? nullptr : &iterator->second;
    }
    const NodeStatus* get_raw_unchecked(const Key& key) const
    {
        const auto iterator = m_index.find(key);
        return iterator == m_index.end() ? nullptr : &iterator->second;
    }
    void set_raw_unchecked(const Key& key, const NodeStatus status) { m_index[key] = status; }
    void remove_raw_unchecked(const Key& key) { m_index.erase(key); }

    Expected<bool> update_parent_after_remove(const Key& key)
    {
        const auto parent = Traits::parent(key);
        if (!parent.has_value()) {
            return true;
        }
        if (!Traits::is_valid(parent.value())) {
            return store::invalid_key_error<Traits>(parent.value());
        }

        NodeStatus* parent_status = get_raw_unchecked(parent.value());
        DEBUG_ASSERT(parent_status != nullptr);
        const auto siblings = Traits::children(parent.value());
        DEBUG_ASSERT(siblings.has_value());
        for (const Key& sibling : siblings.value()) {
            if (sibling != key && get_raw_unchecked(sibling) != nullptr) {
                return true;
            }
        }

        switch (*parent_status) {
        case NodeStatus::Inner:
            *parent_status = NodeStatus::Leaf;
            break;
        case NodeStatus::Virtual:
            remove_raw_unchecked(parent.value());
            return update_parent_after_remove(parent.value());
        case NodeStatus::Leaf:
            UNREACHABLE();
        }
        return true;
    }

    Container m_index;
};

} // namespace store
