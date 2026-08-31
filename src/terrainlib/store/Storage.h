#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include <expected>

#include "log.h"
#include "store/IndexFormat.h"
#include "store/RawStorage.h"

namespace store {

struct StorageSettings {
    bool allow_overwrite = false;
};

template <HierarchyTraits Traits, typename NodeData>
class Storage {
public:
    using Key = typename Traits::Key;
    using key_type = Key;
    using value_type = NodeData;

    struct Persistence {
        IndexFormat<Traits> format;
        std::filesystem::path index_path;
        std::string layout_id;
        std::string payload_class;
        std::string codec_selector;
    };

    explicit Storage(RawStorage<Traits, NodeData> raw)
        : m_raw(std::move(raw))
    {
    }

    Storage(RawStorage<Traits, NodeData> raw, Index<Traits> index, Persistence persistence, const bool dirty = false)
        : m_raw(std::move(raw))
        , m_index(std::move(index))
        , m_persistence(std::move(persistence))
        , m_dirty(dirty)
    {
    }

    virtual ~Storage() { finalize_displaced_state(); }

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    Storage(Storage&& other) noexcept
        : m_raw(std::move(other.m_raw))
        , m_index(std::move(other.m_index))
        , m_persistence(std::move(other.m_persistence))
        , m_settings(other.m_settings)
        , m_dirty(std::exchange(other.m_dirty, false))
    {
        other.m_index.reset();
        other.m_persistence.reset();
    }

    Storage& operator=(Storage&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        finalize_displaced_state();
        m_raw = std::move(other.m_raw);
        m_index = std::move(other.m_index);
        m_persistence = std::move(other.m_persistence);
        m_settings = other.m_settings;
        m_dirty = std::exchange(other.m_dirty, false);
        other.m_index.reset();
        other.m_persistence.reset();
        return *this;
    }

    Expected<NodeData> load(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        if (m_index.has_value()) {
            auto status = m_index->get(key);
            if (!status) {
                return Error::propagate(std::move(status), "look up node " + Traits::key_to_string(key) + " in storage index before loading");
            }
            if (!status->has_value() || status->value() == NodeStatus::Virtual) {
                return Error::fail(Error::Code::NotFound, "node " + Traits::key_to_string(key) + " is not physically present in the index");
            }
        }
        return m_raw.load(key);
    }

    Expected<void> save(const Key& key, const NodeData& data)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        auto exists = has(key);
        if (!exists) {
            return Error::propagate(std::move(exists), "check whether node " + Traits::key_to_string(key) + " exists before saving");
        }
        if (exists.value() && !m_settings.allow_overwrite) {
            const auto node_paths = m_raw.paths(key).value();
            return Error::fail(Error::Code::AlreadyExists, "save node to", node_paths.empty() ? m_raw.layout().node_path(key) : node_paths.front());
        }

        auto result = m_raw.save(key, data);
        if (!result) {
            return result;
        }
        if (m_index.has_value()) {
            auto added = m_index->add(key);
            if (!added) {
                return Error::propagate(std::move(added), "add saved node " + Traits::key_to_string(key) + " to storage index");
            }
            m_dirty = m_dirty || added.value();
        }
        return {};
    }

    Expected<void> copy_from(const Key& key, const Storage& source)
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        auto exists = has(key);
        if (!exists) {
            return Error::propagate(std::move(exists), "check whether target node " + Traits::key_to_string(key) + " exists before copying");
        }
        if (exists.value() && !m_settings.allow_overwrite) {
            const auto node_paths = m_raw.paths(key).value();
            return Error::fail(Error::Code::AlreadyExists, "copy node to", node_paths.empty() ? m_raw.layout().node_path(key) : node_paths.front());
        }
        const auto prepare_target = [&]() -> Expected<void> {
            if (!exists.value() || !m_index.has_value()) {
                return {};
            }
            auto removed = m_index->remove(key);
            if (!removed) {
                return Error::propagate(std::move(removed), "remove overwritten node " + Traits::key_to_string(key) + " from storage index");
            }
            m_dirty = m_dirty || removed.value();
            return {};
        };

        auto result = m_raw.copy_from(key, source.m_raw, prepare_target);
        if (!result) {
            return result;
        }
        if (m_index.has_value()) {
            auto added = m_index->add(key);
            if (!added) {
                return Error::propagate(std::move(added), "add copied node " + Traits::key_to_string(key) + " to storage index");
            }
            m_dirty = m_dirty || added.value();
        }
        return {};
    }

    Expected<bool> remove(const Key& key)
    {
        auto removed = m_raw.remove(key);
        if (!removed) {
            return removed;
        }
        if (m_index.has_value()) {
            auto index_removed = m_index->remove(key);
            if (!index_removed) {
                return Error::propagate(std::move(index_removed), "remove node " + Traits::key_to_string(key) + " from storage index");
            }
            m_dirty = m_dirty || index_removed.value();
        }
        return removed.value();
    }

    Expected<bool> has(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return store::invalid_key_error<Traits>(key);
        }
        if (!m_index.has_value()) {
            return m_raw.has(key);
        }
        auto status = m_index->get(key);
        if (!status) {
            return Error::propagate(std::move(status), "look up node " + Traits::key_to_string(key) + " in storage index");
        }
        return status->has_value() && status->value() != NodeStatus::Virtual;
    }

    Expected<std::vector<std::filesystem::path>> paths(const Key& key) const { return m_raw.paths(key); }

    Expected<std::filesystem::path> path_for(const Key& key) const
    {
        auto node_paths = paths(key);
        if (!node_paths) {
            return Error::propagate(std::move(node_paths));
        }
        return node_paths->empty() ? m_raw.layout().node_path(key) : node_paths->front();
    }

    const std::filesystem::path& base_path() const { return m_raw.layout().base_path(); }
    const path_layout::Resolver<Key>& layout() const { return m_raw.layout(); }
    const Codec<NodeData>& codec() const { return m_raw.codec(); }
    std::optional<std::string_view> codec_selector() const
    {
        if (!m_persistence.has_value()) {
            return std::nullopt;
        }
        return m_persistence->codec_selector;
    }

    bool is_indexed() const { return m_index.has_value(); }
    void ensure_indexed()
    {
        if (!m_index.has_value()) {
            m_index.emplace();
            m_dirty = true;
        }
    }

    std::optional<std::reference_wrapper<const Index<Traits>>> index() const
    {
        if (!m_index.has_value()) {
            return std::nullopt;
        }
        return std::cref(m_index.value());
    }

    Expected<void> save_index() const
    {
        if (!m_dirty) {
            return {};
        }
        if (!m_index.has_value() || !m_persistence.has_value()) {
            return Error::fail(Error::Code::Internal, "indexed storage has no persistence configuration");
        }
        const IndexMetadata<Traits> metadata {
            m_index.value(),
            m_persistence->layout_id,
            m_persistence->payload_class,
            m_persistence->codec_selector,
        };
        auto result = m_persistence->format.write(m_persistence->index_path, metadata);
        if (result) {
            m_dirty = false;
        }
        return result;
    }

    Expected<void> save_or_create_index()
    {
        if (!m_index.has_value()) {
            m_index.emplace();
            m_dirty = true;
        }
        return save_index();
    }

    const StorageSettings& settings() const { return m_settings; }
    StorageSettings& settings() { return m_settings; }

protected:
    Index<Traits>& index_mut() { return m_index.value(); }
    const Index<Traits>& index_ref() const { return m_index.value(); }
    bool is_index_dirty() const { return m_dirty; }
    void set_index_dirty(const bool dirty = true) const { m_dirty = dirty; }

private:
    void finalize_displaced_state() noexcept
    {
        if (!m_dirty || !m_index.has_value()) {
            return;
        }
        const auto result = save_index();
        if (!result) {
            LOG_ERROR("Failed to automatically save index: {}", result.error().to_string());
        }
    }

    RawStorage<Traits, NodeData> m_raw;
    std::optional<Index<Traits>> m_index;
    std::optional<Persistence> m_persistence = std::nullopt;
    StorageSettings m_settings;
    mutable bool m_dirty = false;
};

} // namespace store
