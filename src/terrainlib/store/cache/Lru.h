#pragma once

#include <list>
#include <optional>
#include <unordered_map>

#include "store/cache/Interface.h"

namespace store::cache {

template<HierarchyTraits Traits, typename NodeData>
class Lru final : public Interface<Traits, NodeData> {
public:
    using Key = typename Traits::Key;

    explicit Lru(const size_t capacity) : _capacity(capacity) {}

    std::optional<NodeData> get(const Key &key) noexcept override {
        const auto iterator = _values.find(key);
        if (iterator == _values.end()) {
            return std::nullopt;
        }
        _usage.splice(_usage.begin(), _usage, iterator->second.second);
        return iterator->second.first;
    }

    bool put(const Key &key, const NodeData &value) noexcept override {
        const auto iterator = _values.find(key);
        if (iterator != _values.end()) {
            iterator->second.first = value;
            _usage.splice(_usage.begin(), _usage, iterator->second.second);
            return false;
        }
        if (_capacity == 0) {
            return false;
        }
        if (_values.size() == _capacity) {
            _values.erase(_usage.back());
            _usage.pop_back();
        }
        _usage.push_front(key);
        _values.emplace(key, std::pair<NodeData, typename std::list<Key>::iterator>{
                                 value,
                                 _usage.begin(),
                             });
        return true;
    }

    bool remove(const Key &key) noexcept override {
        const auto iterator = _values.find(key);
        if (iterator == _values.end()) {
            return false;
        }
        _usage.erase(iterator->second.second);
        _values.erase(iterator);
        return true;
    }

    bool contains(const Key &key) const noexcept override {
        return _values.contains(key);
    }

private:
    size_t _capacity;
    std::list<Key> _usage;
    std::unordered_map<
        Key,
        std::pair<NodeData, typename std::list<Key>::iterator>,
        typename Traits::Hasher>
        _values;
};

} // namespace store::cache
