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

    std::expected<NodeData, ::Error> load(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(store::invalid_key_error<Traits>(key));
        }
        if (m_index.has_value()) {
            auto status = m_index->get(key);
            if (!status.has_value()) {
                return std::unexpected(std::move(status).error());
            }
            if (!status->has_value() || status->value() == NodeStatus::Virtual) {
                return std::unexpected(
                    ::Error::make(::Error::Code::NotFound, "node " + Traits::key_to_string(key) + " is not physically present in the index"));
            }
        }
        return m_raw.load(key);
    }

    std::expected<void, ::Error> save(const Key& key, const NodeData& data)
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(store::invalid_key_error<Traits>(key));
        }
        auto exists = has(key);
        if (!exists.has_value()) {
            return std::unexpected(std::move(exists).error());
        }
        if (exists.value() && !m_settings.allow_overwrite) {
            const auto node_paths = m_raw.paths(key).value();
            return std::unexpected(::Error::make(::Error::Code::AlreadyExists,
                "save node to",
                node_paths.empty() ? m_raw.layout().node_path(key) : node_paths.front()));
        }

        auto result = m_raw.save(key, data);
        if (!result.has_value()) {
            return result;
        }
        if (m_index.has_value()) {
            auto added = m_index->add(key);
            if (!added.has_value()) {
                return std::unexpected(std::move(added).error());
            }
            m_dirty = m_dirty || added.value();
        }
        return {};
    }

    std::expected<void, ::Error> copy_from(const Key& key, const Storage& source)
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(store::invalid_key_error<Traits>(key));
        }
        auto exists = has(key);
        if (!exists.has_value()) {
            return std::unexpected(std::move(exists).error());
        }
        if (exists.value() && !m_settings.allow_overwrite) {
            const auto node_paths = m_raw.paths(key).value();
            return std::unexpected(::Error::make(::Error::Code::AlreadyExists,
                "copy node to",
                node_paths.empty() ? m_raw.layout().node_path(key) : node_paths.front()));
        }
        const auto prepare_target = [&]() -> std::expected<void, ::Error> {
            if (!exists.value() || !m_index.has_value()) {
                return {};
            }
            auto removed = m_index->remove(key);
            if (!removed.has_value()) {
                return std::unexpected(std::move(removed).error());
            }
            m_dirty = m_dirty || removed.value();
            return {};
        };

        auto result = m_raw.copy_from(key, source.m_raw, prepare_target);
        if (!result.has_value()) {
            return result;
        }
        if (m_index.has_value()) {
            auto added = m_index->add(key);
            if (!added.has_value()) {
                return std::unexpected(std::move(added).error());
            }
            m_dirty = m_dirty || added.value();
        }
        return {};
    }

    std::expected<bool, ::Error> remove(const Key& key)
    {
        auto removed = m_raw.remove(key);
        if (!removed.has_value()) {
            return removed;
        }
        if (m_index.has_value()) {
            auto index_removed = m_index->remove(key);
            if (!index_removed.has_value()) {
                return std::unexpected(std::move(index_removed).error());
            }
            m_dirty = m_dirty || index_removed.value();
        }
        return removed.value();
    }

    std::expected<bool, ::Error> has(const Key& key) const
    {
        if (!Traits::is_valid(key)) {
            return std::unexpected(store::invalid_key_error<Traits>(key));
        }
        if (!m_index.has_value()) {
            return m_raw.has(key);
        }
        auto status = m_index->get(key);
        if (!status.has_value()) {
            return std::unexpected(std::move(status).error());
        }
        return status->has_value() && status->value() != NodeStatus::Virtual;
    }

    std::expected<std::vector<std::filesystem::path>, ::Error> paths(const Key& key) const { return m_raw.paths(key); }

    std::expected<std::filesystem::path, ::Error> path_for(const Key& key) const
    {
        auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(std::move(node_paths).error());
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

    std::expected<void, ::Error> save_index() const
    {
        if (!m_dirty) {
            return {};
        }
        if (!m_index.has_value() || !m_persistence.has_value()) {
            return std::unexpected(::Error::make(::Error::Code::Internal, "indexed storage has no persistence configuration"));
        }
        const IndexMetadata<Traits> metadata {
            m_index.value(),
            m_persistence->layout_id,
            m_persistence->payload_class,
            m_persistence->codec_selector,
        };
        auto result = m_persistence->format.write(m_persistence->index_path, metadata);
        if (result.has_value()) {
            m_dirty = false;
        }
        return result;
    }

    std::expected<void, ::Error> save_or_create_index()
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
        if (!result.has_value()) {
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
