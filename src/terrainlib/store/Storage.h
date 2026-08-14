#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

#include <expected>

#include "log.h"
#include "store/IndexFormat.h"
#include "store/RawStorage.h"
#include "store/StorageSettings.h"

namespace store {

template<HierarchyTraits Traits, typename NodeData>
class Storage {
public:
    using Key = typename Traits::Key;
    using key_type = Key;
    using value_type = NodeData;
    using load_error = LoadError<Key>;
    using save_error = SaveError<Key>;

    struct Persistence {
        IndexFormat<Traits> format;
        std::filesystem::path index_path;
        std::string layout_id;
        std::string codec_selector;
    };

    explicit Storage(RawStorage<Traits, NodeData> raw) : _raw(std::move(raw)) {}

    Storage(RawStorage<Traits, NodeData> raw, Persistence persistence)
        : _raw(std::move(raw)), _persistence(std::move(persistence)) {}

    Storage(
        RawStorage<Traits, NodeData> raw,
        Index<Traits> index,
        Persistence persistence)
        : _raw(std::move(raw)),
          _index(std::move(index)),
          _persistence(std::move(persistence)) {}

    virtual ~Storage() {
        finalize_displaced_state();
    }

    Storage(const Storage &) = delete;
    Storage &operator=(const Storage &) = delete;

    Storage(Storage &&other) noexcept
        : _raw(std::move(other._raw)),
          _index(std::move(other._index)),
          _persistence(std::move(other._persistence)),
          _settings(other._settings),
          _dirty(std::exchange(other._dirty, false)) {
        other._index.reset();
        other._persistence.reset();
    }

    Storage &operator=(Storage &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        finalize_displaced_state();
        _raw = std::move(other._raw);
        _index = std::move(other._index);
        _persistence = std::move(other._persistence);
        _settings = other._settings;
        _dirty = std::exchange(other._dirty, false);
        other._index.reset();
        other._persistence.reset();
        return *this;
    }

    std::expected<NodeData, LoadError<Key>> load(const Key &key) const {
        if (!Traits::is_valid(key)) {
            return std::unexpected(LoadError<Key>(InvalidKey<Key>{key}));
        }
        if (_index.has_value()) {
            const auto status = _index->get(key);
            if (!status.has_value()) {
                return std::unexpected(LoadError<Key>(status.error()));
            }
            if (!status->has_value()
                || status->value() == NodeStatus::Virtual) {
                return std::unexpected(LoadError<Key>(CodecError{
                    CodecOperation::Read,
                    CodecErrorCategory::FileNotFound,
                    "node is not physically present in the index",
                }));
            }
        }
        return _raw.load(key);
    }

    std::expected<void, SaveError<Key>> save(const Key &key, const NodeData &data) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(SaveError<Key>(InvalidKey<Key>{key}));
        }
        const auto exists = has(key);
        if (!exists.has_value()) {
            return std::unexpected(std::visit(
                [](const auto &error) -> SaveError<Key> { return error; },
                exists.error()));
        }
        if (exists.value() && !_settings.allow_overwrite) {
            const auto node_paths = _raw.paths(key).value();
            return std::unexpected(SaveError<Key>(AlreadyExists{
                node_paths.empty() ? _raw.layout().node_path(key).path() : node_paths.front(),
            }));
        }

        const auto result = _raw.save(key, data);
        if (!result.has_value()) {
            return result;
        }
        if (_index.has_value()) {
            const auto added = _index->add(key);
            if (!added.has_value()) {
                return std::unexpected(SaveError<Key>(added.error()));
            }
            _dirty = _dirty || added.value();
        }
        return {};
    }

    std::expected<void, CopyError<Key>> copy_from(
        const Key &key,
        const Storage &source) {
        if (!Traits::is_valid(key)) {
            return std::unexpected(CopyError<Key>(InvalidKey<Key>{key}));
        }
        const auto exists = has(key);
        if (!exists.has_value()) {
            return std::unexpected(std::visit(
                [](const auto &error) -> CopyError<Key> { return error; },
                exists.error()));
        }
        if (exists.value() && !_settings.allow_overwrite) {
            const auto node_paths = _raw.paths(key).value();
            return std::unexpected(CopyError<Key>(AlreadyExists{
                node_paths.empty() ? _raw.layout().node_path(key).path() : node_paths.front(),
            }));
        }
        if (exists.value() && _index.has_value()) {
            const auto removed = _index->remove(key);
            if (!removed.has_value()) {
                return std::unexpected(CopyError<Key>(removed.error()));
            }
            _dirty = _dirty || removed.value();
        }

        const auto result = _raw.copy_from(key, source._raw);
        if (!result.has_value()) {
            return result;
        }
        if (_index.has_value()) {
            const auto added = _index->add(key);
            if (!added.has_value()) {
                return std::unexpected(CopyError<Key>(added.error()));
            }
            _dirty = _dirty || added.value();
        }
        return {};
    }

    std::expected<bool, FileOperationError<Key>> remove(const Key &key) {
        const auto removed = _raw.remove(key);
        if (!removed.has_value()) {
            return removed;
        }
        if (_index.has_value()) {
            const auto index_removed = _index->remove(key);
            if (!index_removed.has_value()) {
                return std::unexpected(FileOperationError<Key>(index_removed.error()));
            }
            _dirty = _dirty || index_removed.value();
        }
        return removed.value();
    }

    std::expected<bool, FileOperationError<Key>> has(const Key &key) const {
        if (!Traits::is_valid(key)) {
            return std::unexpected(FileOperationError<Key>(InvalidKey<Key>{key}));
        }
        if (!_index.has_value()) {
            return _raw.has(key);
        }
        const auto status = _index->get(key);
        if (!status.has_value()) {
            return std::unexpected(FileOperationError<Key>(status.error()));
        }
        return status->has_value() && status->value() != NodeStatus::Virtual;
    }

    std::expected<std::vector<std::filesystem::path>, InvalidKey<Key>> paths(
        const Key &key) const {
        return _raw.paths(key);
    }

    std::expected<std::filesystem::path, InvalidKey<Key>> path_for(
        const Key &key) const {
        const auto node_paths = paths(key);
        if (!node_paths.has_value()) {
            return std::unexpected(node_paths.error());
        }
        return node_paths->empty()
            ? _raw.layout().node_path(key).path()
            : node_paths->front();
    }

    const std::filesystem::path &base_path() const {
        return _raw.layout().base_path();
    }
    const Layout<Key> &layout() const {
        return _raw.layout();
    }
    const Codec<NodeData> &codec() const {
        return _raw.codec();
    }
    std::optional<std::string_view> codec_selector() const {
        if (!_persistence.has_value()) {
            return std::nullopt;
        }
        return _persistence->codec_selector;
    }

    bool is_indexed() const {
        return _index.has_value();
    }
    void ensure_indexed() {
        if (!_index.has_value()) {
            _index.emplace();
            _dirty = true;
        }
    }

    std::optional<std::reference_wrapper<const Index<Traits>>> index() const {
        if (!_index.has_value()) {
            return std::nullopt;
        }
        return std::cref(_index.value());
    }

    std::expected<void, IndexFormatError> save_index() const {
        if (!_dirty) {
            return {};
        }
        if (!_index.has_value() || !_persistence.has_value()) {
            return std::unexpected(IndexFormatError{
                IndexFormatErrorCategory::Write,
                {},
                "indexed storage has no persistence configuration",
            });
        }
        const IndexMetadata<Traits> metadata{
            _index.value(),
            _persistence->layout_id,
            _persistence->codec_selector,
        };
        const auto result = _persistence->format.write(
            _persistence->index_path,
            metadata);
        if (result.has_value()) {
            _dirty = false;
        }
        return result;
    }

    std::expected<void, IndexFormatError> save_or_create_index() {
        if (!_index.has_value()) {
            const auto scan_result = scan_index();
            if (!scan_result.has_value()) {
                return std::unexpected(scan_result.error());
            }
            _index = std::move(scan_result.value());
            _dirty = true;
        }
        return save_index();
    }

    const StorageSettings &settings() const {
        return _settings;
    }
    StorageSettings &settings() {
        return _settings;
    }

protected:
    Index<Traits> &index_mut() {
        return _index.value();
    }
    const Index<Traits> &index_ref() const {
        return _index.value();
    }
    bool is_index_dirty() const {
        return _dirty;
    }
    void set_index_dirty(const bool dirty = true) const {
        _dirty = dirty;
    }

private:
    std::expected<Index<Traits>, IndexFormatError> scan_index() const {
        std::error_code error;
        std::filesystem::create_directories(base_path(), error);
        if (error) {
            return std::unexpected(IndexFormatError{
                IndexFormatErrorCategory::Open,
                base_path(),
                error.message(),
            });
        }

        const NodePath probe("__codec_probe__/node");
        const std::string probe_text = probe.path().generic_string();
        std::vector<std::string> suffixes;
        for (const auto &codec_path : _raw.codec().paths(probe)) {
            const std::string text = codec_path.generic_string();
            if (text.starts_with(probe_text)) {
                suffixes.push_back(text.substr(probe_text.size()));
            }
        }

        std::unordered_set<Key, typename Traits::Hasher> candidates;
        for (std::filesystem::recursive_directory_iterator iterator(base_path(), error), end;
             !error && iterator != end;
             iterator.increment(error)) {
            if (!iterator->is_regular_file(error) && !iterator->is_symlink(error)) {
                if (error) {
                    break;
                }
                continue;
            }
            const std::string path_text = iterator->path().generic_string();
            for (const std::string &suffix : suffixes) {
                if (path_text.size() < suffix.size()
                    || !path_text.ends_with(suffix)) {
                    continue;
                }
                const NodePath node_path(
                    std::filesystem::path(path_text.substr(0, path_text.size() - suffix.size())));
                const auto key = _raw.layout().key_from_node_path(node_path);
                if (key.has_value() && Traits::is_valid(key.value())) {
                    candidates.insert(key.value());
                }
            }
        }
        if (error) {
            return std::unexpected(IndexFormatError{
                IndexFormatErrorCategory::Open,
                base_path(),
                error.message(),
            });
        }

        Index<Traits> result;
        for (const Key &key : candidates) {
            const auto present = _raw.has(key);
            if (!present.has_value()) {
                const FilesystemError &filesystem_error =
                    std::get<FilesystemError>(present.error());
                return std::unexpected(IndexFormatError{
                    IndexFormatErrorCategory::Open,
                    filesystem_error.path,
                    filesystem_error.error.message(),
                });
            }
            if (present.value()) {
                const auto added = result.add(key);
                if (!added.has_value()) {
                    return std::unexpected(IndexFormatError{
                        IndexFormatErrorCategory::Malformed,
                        base_path(),
                        "codec path resolved to an invalid hierarchy key",
                    });
                }
            }
        }
        return result;
    }

    void finalize_displaced_state() noexcept {
        if (!_dirty || !_index.has_value()) {
            return;
        }
        const auto result = save_index();
        if (!result.has_value()) {
            LOG_ERROR(
                "Failed to automatically save index {}: {}",
                result.error().path,
                result.error().message);
        }
    }

    RawStorage<Traits, NodeData> _raw;
    std::optional<Index<Traits>> _index;
    std::optional<Persistence> _persistence;
    StorageSettings _settings;
    mutable bool _dirty = false;
};

} // namespace store
