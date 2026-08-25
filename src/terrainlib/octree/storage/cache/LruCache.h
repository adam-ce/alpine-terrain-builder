#pragma once

#include <optional>
#include <list>
#include <unordered_map>

#include "octree/Id.h"
#include "octree/RawStorage.h"
#include "octree/cache/ICache.h"

namespace octree::cache {

// TODO: UNTESTED
// Do not use this cache for NodeLoader's ancestor lookup until cached ancestors
// are clipped to the requested node bounds. NodeLoader currently returns a
// cached ancestor unchanged, producing incorrect geometry for child requests.
template <typename Key, typename Value>
class Lru_ : public ICache {
public:
    explicit Lru_(const size_t capacity) : _capacity(capacity) {}

    std::optional<Value> get(const Key& key) {
        auto it = this->_map.find(key);
        if (it == this->_map.end()) return std::nullopt;

        // Move to front (most recently used)
        this->_usage.splice(this->_usage.begin(), this->_usage, it->second.second);
        return it->second.first;
    }

    void put(const Key& key, const Value& value) {
        auto it = this->_map.find(key);
        if (it != this->_map.end()) {
            // Update value and move to front
            it->second.first = value;
            this->_usage.splice(this->_usage.begin(), this->_usage, it->second.second);
        } else {
            // Insert new entry
            if (this->_map.size() == _capacity) {
                // Evict least recently used
                const Key& lru_key = this->_usage.back();
                this->_map.erase(lru_key);
                this->_usage.pop_back();
            }

            this->_usage.push_front(key);
            this->_map[key] = { value, this->_usage.begin() };
        }
    }

    void remove(const Key& key) {
        auto it = this->_map.find(key);
        if (it != this->_map.end()) {
            this->_usage.erase(it->second.second);
            this->_map.erase(it);
        }
    }

    bool contains(const Key& key) const {
        return this->_map.contains(key);
    }

private:
    size_t _capacity;
    std::list<Key> _usage;
    std::unordered_map<Key, std::pair<Value, typename std::list<Key>::iterator>> _map;
};

using Lru<T> = Lru<octree::Id, T>;

}
