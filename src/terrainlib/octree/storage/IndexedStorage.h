#pragma once

#include <filesystem>

#include <libassert/assert.hpp>

#include "octree/Id.h"
#include "octree/storage/helpers.h"
#include "octree/IndexMap.h"
#include "octree/storage/Storage.h"
#include "octree/disk/Layout.h"

namespace octree {

class IndexedStorage : public Storage {
public:
    explicit IndexedStorage(Storage inner) noexcept
        : Storage(std::move(inner)) {
        this->ensure_indexed();
        DEBUG_ASSERT(this->is_indexed());
    }
    explicit IndexedStorage(RawStorage inner, IndexMap map) noexcept
        : Storage(std::move(inner), std::move(map)) {
        DEBUG_ASSERT(this->is_indexed());
    }

    IndexedStorage &operator=(const IndexedStorage &) = delete;
    IndexedStorage(const IndexedStorage &) = delete;
    IndexedStorage(IndexedStorage &&) = default;
    IndexedStorage &operator=(IndexedStorage &&) = default;

    ~IndexedStorage() override {
        if (this->is_index_dirty()) {
            LOG_WARN("Index was not saved upon IndexedStorage destruction, use save_index().");
        }
    }

    const IndexMap& index() const noexcept {
        DEBUG_ASSERT(this->is_indexed());
        return Storage::index().value();
    }
    void update_index() noexcept {
        Storage::update_index();
    }
    tl::expected<void, io::Error> save_index() const noexcept {
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
        return Storage::index_mut().value();
    }
};

} // namespace octree
