#pragma once

#include <optional>

#include <zpp_bits.h>

#include "octree/NodeStatus.h"
#include "octree/StoreTraits.h"
#include "store/Index.h"

namespace octree {

class IndexMap {
public:
    using SharedIndex = store::Index<StoreTraits>;
    using iterator = SharedIndex::iterator;
    using const_iterator = SharedIndex::const_iterator;

    IndexMap() = default;
    explicit IndexMap(SharedIndex index) : _index(std::move(index)) {}

    std::optional<NodeStatus> get(const Id id) const {
        return _index.get(id).value();
    }
    bool add(const Id id) {
        return _index.add(id).value();
    }
    bool remove(const Id id) {
        return _index.remove(id).value();
    }
    bool is_present(const Id id) const {
        return _index.is_present(id).value();
    }
    bool is_absent(const Id id) const {
        return _index.is_absent(id).value();
    }
    bool is(const NodeStatus status, const Id id) const {
        return _index.is(status, id).value();
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

    NodeStatus *get_raw(const Id id) {
        return _index.get_raw(id).value();
    }
    const NodeStatus *get_raw(const Id id) const {
        return _index.get_raw(id).value();
    }
    void set_raw(const Id id, const NodeStatus status) {
        _index.set_raw(id, status).value();
    }
    void remove_raw(const Id id) {
        _index.remove_raw(id).value();
    }

    SharedIndex &shared() {
        return _index;
    }
    const SharedIndex &shared() const {
        return _index;
    }
    SharedIndex take_shared() && {
        return std::move(_index);
    }

    using serialize = zpp::bits::members<1>;
    friend zpp::bits::access;

private:
    SharedIndex _index;
};

} // namespace octree
