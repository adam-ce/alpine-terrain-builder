#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <unordered_map>

#include <libassert/assert.hpp>
#include <zpp_bits.h>

#include "log.h"
#include "store/InvalidKey.h"
#include "store/NodeStatus.h"
#include "store/Traits.h"

namespace store {

template<HierarchyTraits Traits>
class Index {
public:
    using Key = typename Traits::Key;
    using Error = InvalidKey<Key>;
    using Container = std::unordered_map<Key, NodeStatus, typename Traits::Hasher>;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;

    std::expected<std::optional<NodeStatus>, Error> get(const Key &key) const {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        const NodeStatus *status = get_raw_unchecked(key);
        if (status == nullptr) {
            return std::nullopt;
        }
        return *status;
    }

    std::expected<bool, Error> add(const Key &key) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }

        NodeStatus *status = get_raw_unchecked(key);
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
            return std::unexpected(Error{parent.value()});
        }
        NodeStatus *parent_status = get_raw_unchecked(parent.value());
        if (parent_status == nullptr) {
            const auto parent_result = add(parent.value());
            if (!parent_result.has_value()) {
                return std::unexpected(parent_result.error());
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

    std::expected<bool, Error> remove(const Key &key) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }

        NodeStatus *status = get_raw_unchecked(key);
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

    std::expected<bool, Error> is_present(const Key &key) const {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        return _index.contains(key);
    }
    std::expected<bool, Error> is_absent(const Key &key) const {
        const auto present = is_present(key);
        if (!present.has_value()) {
            return std::unexpected(present.error());
        }
        return !present.value();
    }
    std::expected<bool, Error> is(const NodeStatus status, const Key &key) const {
        const auto result = get(key);
        if (!result.has_value()) {
            return std::unexpected(result.error());
        }
        return result.value() == status;
    }

    std::expected<NodeStatus *, Error> get_raw(const Key &key) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        return get_raw_unchecked(key);
    }
    std::expected<const NodeStatus *, Error> get_raw(const Key &key) const {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        return get_raw_unchecked(key);
    }
    std::expected<void, Error> set_raw(const Key &key, const NodeStatus status) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        set_raw_unchecked(key, status);
        return {};
    }
    std::expected<void, Error> remove_raw(const Key &key) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(Error{key});
        }
        remove_raw_unchecked(key);
        return {};
    }

    void clear() {
        _index.clear();
    }
    bool empty() const {
        return _index.empty();
    }
    size_t size() const {
        return _index.size();
    }

    const_iterator begin() const {
        return _index.begin();
    }
    const_iterator end() const {
        return _index.end();
    }
    const_iterator cbegin() const {
        return _index.cbegin();
    }
    const_iterator cend() const {
        return _index.cend();
    }

    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;

private:
    NodeStatus *get_raw_unchecked(const Key &key) {
        const auto iterator = _index.find(key);
        return iterator == _index.end() ? nullptr : &iterator->second;
    }
    const NodeStatus *get_raw_unchecked(const Key &key) const {
        const auto iterator = _index.find(key);
        return iterator == _index.end() ? nullptr : &iterator->second;
    }
    void set_raw_unchecked(const Key &key, const NodeStatus status) {
        _index[key] = status;
    }
    void remove_raw_unchecked(const Key &key) {
        _index.erase(key);
    }

    std::expected<bool, Error> update_parent_after_remove(const Key &key) {
        const auto parent = Traits::parent(key);
        if (!parent.has_value()) {
            return true;
        }
        if (!Traits::is_valid(parent.value())) {
            return std::unexpected(Error{parent.value()});
        }

        NodeStatus *parent_status = get_raw_unchecked(parent.value());
        DEBUG_ASSERT(parent_status != nullptr);
        const auto siblings = Traits::children(parent.value());
        DEBUG_ASSERT(siblings.has_value());
        for (const Key &sibling : siblings.value()) {
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

    Container _index;
};

} // namespace store
