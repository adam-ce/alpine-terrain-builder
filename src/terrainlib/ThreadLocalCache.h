#include <thread>
#include <unordered_map>

template <typename Key, typename Value>
class ThreadLocalCache {
public:
    template <typename Func>
    const Value &get_or_add(const Key &key, Func &&func) {
        auto it = this->_cache.find(key);
        if (it != this->_cache.end()) {
            return it->second;
        }

        auto [new_it, _] = this->_cache.emplace(key, func());
        return new_it->second;
    }

    std::optional<const std::reference_wrapper<const Value>> try_get(const Key &key) {
        auto it = this->_cache.find(key);
        if (it != this->_cache.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    thread_local std::unordered_map<Key, Value> _cache;
};
