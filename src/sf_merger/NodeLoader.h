#pragma once

#include <optional>

#include <libassert/assert.hpp>

#include "NodeLoader.h"
#include "mesh/SimpleMesh.h"
#include "mesh/clip.h"
#include "mesh/texture_trim.h"
#include "octree/Id.h"
#include "octree/NodeStatusOrMissing.h"
#include "octree/Space.h"
#include "octree/Storage.h"
#include "octree/storage/cache/Dummy.h"
#include "octree/storage/cache/ICache.h"

class NodeLoader {
public:
    NodeLoader(const octree::IndexedStorage &storage, octree::cache::ICache<mesh::Simple> &cache, octree::Space space)
        : _storage(storage), _cache(cache), _space(space) {}

    octree::NodeStatusOrMissing get_status(const octree::Id &id) const noexcept {
        return octree::NodeStatusOrMissing(
            DEBUG_ASSERT_VAL(this->_storage.index().get(id)).value());
    }

    // Try to retrieve or reconstruct node mesh by ID
    // TODO: return const ref instead?
    std::optional<SimpleMesh> load_node(const octree::Id &id) const noexcept {
        // Try cache
        if (auto cached = this->_cache.get(id); cached.has_value()) {
            return cached.value();
        }

        // Try storage
        auto mesh_opt = this->_storage.load(id);
        if (mesh_opt.has_value()) {
            auto mesh = mesh_opt.value();
            this->_cache.put(id, mesh);
            return mesh;
        }

        // Try ancestors
        for (auto parent = id.parent(); parent.has_value(); parent = parent->parent()) {
            const auto &parent_id = *parent;

            // Try cache
            if (auto cached = this->_cache.get(parent_id); cached.has_value()) {
                return cached;
            }

            // Try storage
            auto parent_mesh_opt = this->_storage.load(parent_id);
            if (parent_mesh_opt.has_value()) {
                auto parent_mesh = parent_mesh_opt.value();
                const auto bounds = this->_space.get_node_bounds(id);
                SimpleMesh clipped = mesh::clip_on_bounds(parent_mesh, bounds);
                trim_texture_inplace(clipped);
                this->_cache.put(id, clipped);
                return clipped;
            }
        }

        // Nothing found
        return std::nullopt;
    }

    const octree::IndexedStorage &storage() const {
        return this->_storage;
    }

private:
    const octree::IndexedStorage &_storage;
    octree::cache::ICache<mesh::Simple> &_cache;
    const octree::Space _space;
};
