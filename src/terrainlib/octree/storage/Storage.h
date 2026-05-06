#pragma once

#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include <tl/expected.hpp>

#include "mesh/io.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/disk/Layout.h"
#include "octree/disk/layout/strategy/Default.h"
#include "octree/storage/RawStorage.h"
#include "octree/storage/cache/ICache.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"
#include "octree/storage/helpers.h"

namespace octree {

namespace detail {
struct MaybeIndex {
    std::optional<IndexMap> map;
    mutable bool dirty = false;

    explicit MaybeIndex() : map(std::nullopt) {}
    explicit MaybeIndex(IndexMap map) : map(std::move(map)) {}

    bool add(const Id &id) noexcept {
        if (!map.has_value()) {
            return false;
        }

        const bool changed = map->add(id);
        dirty = dirty || changed;
        return changed;
    }

    bool remove(const Id &id) noexcept {
        if (!map.has_value()) {
            return false;
        }

        const bool changed = map->remove(id);
        dirty = dirty || changed;
        return changed;
    }

    bool contains(const Id &id, const bool def = false) const noexcept {
        if (this->map.has_value()) {
            return this->map->is_present(id);
        }
        return def;
    }
};

template <typename T>
struct MaybeCache : public cache::ICache<T> {
    std::optional<std::unique_ptr<cache::ICache<T>>> cache;

    explicit MaybeCache() : cache(std::nullopt) {}
    explicit MaybeCache(std::unique_ptr<cache::ICache<T>> cache) : cache(std::move(cache)) {}

    std::optional<T> get(const Id& id) noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->get(id);
        }
        return std::nullopt;
    }

    bool put(const Id &id, const T &value) noexcept override {
        if (this->cache.has_value()) {
            return this->cache->get()->put(id, value);
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

struct StorageSettings {
    bool allow_overwrite = false;
};

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class Storage {
public:
    using value_type = T;
    using codec_type = Codec;
    using load_error = typename Codec::load_error;
    using save_error = typename Codec::save_error;

    explicit Storage(RawStorage<T, Codec> inner)
        : _inner(std::move(inner)) {}
    explicit Storage(RawStorage<T, Codec> inner, IndexMap index)
        : _inner(std::move(inner)), _index(detail::MaybeIndex(std::move(index))) {}

    Storage &operator=(const Storage &) = delete;
    Storage(const Storage &) = delete;
    Storage(Storage &&) = default;
    Storage &operator=(Storage &&) = default;

    virtual ~Storage() {
        if (this->_index.map.has_value() && this->_index.dirty) {
            auto result = helpers::save_index_map(this->_index.map.value(), this->_inner.layout());
            if (!result.has_value()) {
                LOG_ERROR("Failed to automatically save index when closing storage: {}", result.error());
            }
        }
    }

    tl::expected<value_type, load_error> load(const Id &id) const noexcept {
        if (const auto value_opt = this->_cache.get(id)) {
            return value_opt.value();
        }

        if (!this->_index.contains(id, true)) {
            return tl::unexpected(Codec::file_not_found());
        }

        const auto result = this->_inner.load(id);
        if (result.has_value()) {
            this->_cache.put(id, result.value());
        }
        return result;
    }

    tl::expected<void, save_error> save(const Id &id, const value_type &value) noexcept {
        if (!this->check_overwrite(id)) {
            // TODO: proper error
            LOG_ERROR_AND_EXIT("tried to overwrite value when not allowed");
        }

        const auto result = this->_inner.save(id, value);
        if (result.has_value()) {
            this->_cache.put(id, value);
            this->_index.add(id);
        }
        return result;
    }

    tl::expected<void, CopyError> copy_from(const Id &id, const Storage<T, Codec> &source) noexcept {
        if (!source._index.contains(id, true)) {
            return tl::unexpected(CopyErrorKind::FileNotFound);
        }

        if (!this->check_overwrite(id)) {
            // TODO: proper error
            LOG_ERROR_AND_EXIT("tried to overwrite value when not allowed");
        }

        const auto result = this->_inner.copy_from(id, source._inner);
        if (result.has_value() && this->is_indexed()) {
            auto& index = this->_index.map.value();
            index.add(id);
            this->set_index_dirty();
        }
        return result;
    }

    tl::expected<void, CopyError> copy_to(const Id &id, Storage<T, Codec> &target) const noexcept {
        return target.copy_from(id, *this);
    }

    bool remove(const Id &id) noexcept {
        this->_cache.remove(id);
        this->_index.remove(id);
        return this->_inner.remove(id);
    }
  
    bool has(const Id &id) const noexcept {
        return 
            this->_cache.contains(id) ||
            this->_index.contains(id, false) ||
            this->_inner.has(id);
    }

    std::filesystem::path path_for(const Id &id) const noexcept {
        return this->_inner.path_for(id);
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

    std::optional<std::unique_ptr<cache::ICache<T>>> &cache() noexcept {
        return this->_cache.cache;
    }

    std::optional<std::reference_wrapper<const cache::ICache<T>>> cache() const noexcept {
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

    const StorageSettings& settings() const noexcept {
        return this->_settings;
    }
    StorageSettings& settings() noexcept {
        return this->_settings;
    }

protected:
    bool check_overwrite(const Id &id) noexcept {
        return !this->_settings.allow_overwrite && this->has(id);
    }

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
    RawStorage<T, Codec> _inner;
    detail::MaybeIndex _index = detail::MaybeIndex();
    mutable detail::MaybeCache<T> _cache = detail::MaybeCache<T>();
    StorageSettings _settings = {};
};

} // namespace octree
