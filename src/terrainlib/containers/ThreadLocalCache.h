/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2025 Martin Braunsperger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

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
