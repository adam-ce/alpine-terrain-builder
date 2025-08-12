#pragma once

#include <filesystem>
#include <optional>
#include <list>
#include <unordered_map>

#include <tl/expected.hpp>

#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/storage/cache/ICache.h"
#include "octree/storage/RawStorage.h"
#include "octree/storage/helpers.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/strategy/Default.h"

namespace octree {

namespace detail {
struct MaybeIndex {
    std::optional<IndexMap> map;
    mutable bool dirty = false;

    explicit MaybeIndex()
        : map(std::nullopt) {}
    explicit MaybeIndex(IndexMap map)
        : map(std::move(map)) {}

    bool add(const Id &id) noexcept {
        if (this->map.has_value()) {
            this->dirty = true;
            return this->map->add(id);
        }
        return false;
    }

    bool remove(const Id &id) noexcept {
        if (this->map.has_value()) {
            this->dirty = true;
            return this->map->remove(id);
        }
        return false;
    }

    bool contains(const Id &id, const bool def = false) const noexcept {
        if (this->map.has_value()) {
            return this->map->is_present(id);
        }
        return def;
    }
};

struct MaybeCache : public cache::ICache {
    std::optional<std::unique_ptr<ICache>> cache;

    explicit MaybeCache()
        : cache(std::nullopt) {}
    explicit MaybeCache(std::unique_ptr<ICache> cache)
        : cache(std::move(cache)) {}

    std::optional<Node> get(const Id& id) noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->get(id);
        }
        return std::nullopt;
    }

    bool put(const Id &id, const Node &node) noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->put(id, node);
        }
        return false;
    }

    bool remove(const Id &id) noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->remove(id);
        }
        return false;
    }

    bool contains(const Id &id) const noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->contains(id);
        }
        return false;
    }
};
}

class Storage {
public:
    explicit Storage(RawStorage inner)
        : _inner(std::move(inner)) {}
    explicit Storage(RawStorage inner, IndexMap index)
        : _inner(std::move(inner)), _index(detail::MaybeIndex(std::move(index))) {}

    Storage& operator=(const Storage&) = delete;
    Storage(const Storage&) = delete;
    Storage(Storage&&) = default;
    Storage& operator=(Storage&&) = default;

    virtual ~Storage() {
        if (this->_index.map.has_value() && this->_index.dirty) {
            auto result = helpers::save_index_map(this->_index.map.value(), this->_inner.layout());
            if (!result.has_value()) {
                LOG_ERROR("Failed to automatically save index when closing storage: {}", result.error());
            }
        }
    }

    tl::expected<Node, mesh::io::LoadMeshError> read_node(const Id &id) const noexcept {
        if (const auto node_opt = this->_cache.get(id)) {
            return node_opt.value();
        }

        if (!this->_index.contains(id, true)) {
            return tl::unexpected(mesh::io::LoadMeshErrorKind::FileNotFound);
        }

        const auto result = this->_inner.read_node(id);
        if (result.has_value()) {
            this->_cache.put(id, result.value());
        }
        return result;
    }

    tl::expected<void, mesh::io::SaveMeshError> write_node(const Id &id, const Node &node, const bool overwrite = false) noexcept {
        // TODO: implement overwrite
        const auto result = this->_inner.write_node(id, node);
        if (result.has_value()) {
            this->_cache.put(id, node);
            this->_index.add(id);
        }
        return result;
    }

    tl::expected<void, CopyMeshError> copy_node_to(const Id &id, Storage &target) const {
        if (!this->_index.contains(id, true)) {
            return tl::unexpected(CopyMeshErrorKind::FileNotFound);
        }

        const auto result = this->_inner.copy_node_to(id, target._inner);
        if (result.has_value() && target.is_indexed()) {
            auto& index = target.index_mut().value();
            index.add(id);
            target.set_index_dirty();
        }
        return result;
    }

    bool remove_node(const Id &id) noexcept {
        this->_cache.remove(id);
        this->_index.remove(id);
        return this->_inner.remove_node(id);
    }
  
    bool has_node(const Id &id) const noexcept {
        return 
            this->_cache.contains(id) ||
            this->_index.contains(id, false) ||
            this->_inner.has_node(id);
    }

    std::filesystem::path get_node_path(const Id &id) const noexcept {
        return this->_inner.get_node_path(id);
    }

    std::filesystem::path base_path() const noexcept {
        return this->_inner.base_path();
    }

    bool is_indexed() const noexcept {
        return this->_index.map.has_value();
    }

    void ensure_indexed() noexcept {
        if (this->is_indexed()) {
            return;
        }

        LOG_TRACE("Index not present, creating empty index");
        IndexMap map;
        helpers::update_index_map(map, this->_inner.layout());
        LOG_TRACE("Index created with {} entries", map.size());
        this->_index.map = std::move(map);
    }
    
    std::optional<std::reference_wrapper<const IndexMap>> index() const noexcept {
        // I miss Option::as_ref
        if (this->_index.map.has_value()) {
            return this->_index.map.value();
        } else {
            return std::nullopt;
        }
    }

    std::optional<std::unique_ptr<cache::ICache>> &cache() noexcept {
        return this->_cache.cache;
    }

    std::optional<std::reference_wrapper<const cache::ICache>> cache() const noexcept {
        if (this->_cache.cache.has_value()) {
            return *this->_cache.cache.value();
        } else {
            return std::nullopt;
        }
    }

    tl::expected<void, io::Error> save_or_create_index() noexcept {
        if (this->is_indexed() && !this->_index.dirty) {
            return {};
        }

        this->ensure_indexed();

        auto result = helpers::save_index_map(this->index().value(), this->_inner.layout());
        if (result.has_value()) {
            this->_index.dirty = false;
        }
        return result;
    }

    const octree::disk::Layout &layout() const noexcept {
        return this->_inner.layout();
    }

protected:
    std::optional<IndexMap> &index_mut() noexcept {
        return this->_index.map;
    }

    bool is_index_dirty() const noexcept {
        return this->_index.dirty;
    }
    void set_index_dirty(const bool val = true) const noexcept {
        this->_index.dirty = val;
    }

    void update_index() noexcept {
        if (!this->is_indexed()) {
            return;
        }

        helpers::update_index_map(this->index_mut().value(), this->_inner.layout());
        this->_index.dirty = false;
    }

private:
    RawStorage _inner;
    detail::MaybeIndex _index = detail::MaybeIndex();
    mutable detail::MaybeCache _cache = detail::MaybeCache();
};

} // namespace octree
