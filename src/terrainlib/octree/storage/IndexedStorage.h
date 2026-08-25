#pragma once

#include <filesystem>

#include <libassert/assert.hpp>

#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "octree/disk/Layout.h"
#include "octree/storage/Storage.h"
#include "octree/storage/codec/Codec.h"
#include "octree/storage/defaults.h"
#include "octree/storage/helpers.h"

namespace octree {

template <typename T = DefaultT, CodecFor<T> Codec = DefaultCodecFor<T>>
class IndexedStorage_ : public Storage_<T, Codec> {
public:
    explicit IndexedStorage_(Storage_<T, Codec> inner) noexcept
        : Storage_<T, Codec>(std::move(inner)) {
        this->ensure_indexed();
        DEBUG_ASSERT(this->is_indexed());
    }
    explicit IndexedStorage_(RawStorage_<T, Codec> inner, IndexMap map) noexcept
        : Storage_<T, Codec>(std::move(inner), std::move(map)) {
        DEBUG_ASSERT(this->is_indexed());
    }

    IndexedStorage_<T, Codec> &operator=(const IndexedStorage_<T, Codec> &) = delete;
    IndexedStorage_(const IndexedStorage_<T, Codec> &) = delete;
    IndexedStorage_(IndexedStorage_<T, Codec> &&) = default;
    IndexedStorage_<T, Codec> &operator=(IndexedStorage_<T, Codec> &&) = default;

    ~IndexedStorage_() override {
        if (this->is_index_dirty()) {
            LOG_WARN("Index was not saved upon IndexedStorage_ destruction, use save_index().");
        }
    }

    const IndexMap& index() const noexcept {
        DEBUG_ASSERT(this->is_indexed());
        return Storage_<T, Codec>::index().value();
    }
    void update_index() noexcept {
        Storage_<T, Codec>::update_index();
    }
    std::expected<void, io::Error> save_index() const noexcept {
        if (!this->is_index_dirty()) {
            return {};
        }

        auto result = helpers::save_index_map(this->index(), this->layout());
        if (result.has_value()) {
            this->set_index_dirty(false);
        }
        return result;
    }

private:
    IndexMap& index_mut() noexcept {
        DEBUG_ASSERT(this->is_indexed());
        return Storage_<T, Codec>::index_mut().value();
    }
};

} // namespace octree
