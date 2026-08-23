#pragma once

#include <list>
#include <optional>
#include <unordered_map>

#include "store/cache/Interface.h"

namespace store::cache {

template <HierarchyTraits Traits, typename NodeData>
class Lru final : public Interface<Traits, NodeData> {
public:
    using Key = typename Traits::Key;

    explicit Lru(const size_t capacity)
        : m_capacity(capacity)
    {
    }

    std::optional<NodeData> get(const Key& key) noexcept override
    {
        const auto iterator = m_values.find(key);
        if (iterator == m_values.end()) {
            return std::nullopt;
        }
        m_usage.splice(m_usage.begin(), m_usage, iterator->second.second);
        return iterator->second.first;
    }

    bool put(const Key& key, const NodeData& value) noexcept override
    {
        const auto iterator = m_values.find(key);
        if (iterator != m_values.end()) {
            iterator->second.first = value;
            m_usage.splice(m_usage.begin(), m_usage, iterator->second.second);
            return false;
        }
        if (m_capacity == 0) {
            return false;
        }
        if (m_values.size() == m_capacity) {
            m_values.erase(m_usage.back());
            m_usage.pop_back();
        }
        m_usage.push_front(key);
        m_values.emplace(key,
            std::pair<NodeData, typename std::list<Key>::iterator> {
                value,
                m_usage.begin(),
            });
        return true;
    }

    bool remove(const Key& key) noexcept override
    {
        const auto iterator = m_values.find(key);
        if (iterator == m_values.end()) {
            return false;
        }
        m_usage.erase(iterator->second.second);
        m_values.erase(iterator);
        return true;
    }

    bool contains(const Key& key) const noexcept override { return m_values.contains(key); }

private:
    size_t m_capacity;
    std::list<Key> m_usage;
    std::unordered_map<Key, std::pair<NodeData, typename std::list<Key>::iterator>, typename Traits::Hasher> m_values;
};

} // namespace store::cache
