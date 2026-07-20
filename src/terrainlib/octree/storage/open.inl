#include <algorithm>
#include <filesystem>
#include <memory>
#include <unordered_set>
#include <vector>

#include <expected>

#include "io/serialize.h"
#include "log.h"
#include "io/Error.h"
#include "octree/disk/IndexFile.h"
#include "octree/disk/layout/StrategyRegister.h"
#include "octree/storage/IndexedStorage.h"
#include "octree/storage/helpers.h"
#include "octree/storage/defaults.h"
#include "range_utils.h"

namespace octree {

template <typename T, CodecFor<T> Codec>
std::expected<IndexedStorage_<T, Codec>, io::Error> open_index(const std::filesystem::path &index_path) {
    LOG_TRACE("Opening storage index {}", index_path);

    const auto result = io::read_from_path<disk::v1::IndexFile>(index_path);
    if (!result.has_value()) {
        LOG_TRACE("Failed to open storage index due to {}", result.error());
        return std::unexpected(result.error());
    }
    auto index_file = result.value();
    LOG_TRACE("Successfully read storage index with {} entries.", index_file.map.size());
    auto index_map = std::move(index_file.map);
    const auto base_path = index_path.parent_path();
    auto layout_strategy = disk::layout::StrategyRegister::instance().create(index_file.layout_strategy_id);
    disk::Layout layout(base_path, std::move(layout_strategy), index_file.preferred_extension);
    RawStorage_<T, Codec> raw_storage(std::move(layout));
    return IndexedStorage_<T, Codec>(std::move(raw_storage), std::move(index_map));
}

template <typename T, CodecFor<T> Codec>
Storage_<T, Codec> open_folder(
    const std::filesystem::path &base_path,
    bool create_index,
    OpenOptions options) {
    LOG_TRACE("Opening storage folder {}", base_path);

    if (!std::filesystem::is_directory(base_path)) {
        if (std::filesystem::exists(base_path)) {
            LOG_ERROR_AND_EXIT("Base path {} exists but is not a directory", base_path);
        }

        LOG_TRACE("Base path {} does not exist, creating it", base_path);
        std::filesystem::create_directories(base_path);
    }

    const std::filesystem::path index_path = base_path / disk::v1::index_file_name();
    auto storage_opt = open_index<T, Codec>(index_path);
    if (storage_opt.has_value()) {
        LOG_TRACE("Loaded existing index");
        return Storage_<T, Codec>(std::move(storage_opt.value()));
    }

    auto layout_info_opt = helpers::guess_layout_strategy(base_path);
    if (layout_info_opt.has_value()) {
        const octree::helpers::LayoutWithoutBase &layout_info = layout_info_opt.value();
        LOG_TRACE("Guessed layout strategy of dataset as {} and extension as {}",
                  disk::layout::StrategyRegister::instance().get_id(*layout_info.strategy),
                  layout_info.extension_with_dot);
    } else {
        auto default_layout_strategy = std::move(options.default_layout_strategy);
        if (!default_layout_strategy) {
            default_layout_strategy = disk::layout::strategy::make_default();
        }
        std::string default_extension_with_dot;
        const std::vector<std::string_view> supported_extensions = Codec::supported_extensions();
        if (options.preferred_extension_with_dot.has_value()) {
            ASSERT(contains(supported_extensions, options.preferred_extension_with_dot.value()));
            default_extension_with_dot = options.preferred_extension_with_dot.value();
        } else {
            default_extension_with_dot = supported_extensions[0];
        }
        LOG_WARN("Unable to determine layout of dataset, using layout strategy {} and extension {}",
                 disk::layout::StrategyRegister::instance().get_id(*default_layout_strategy),
                 default_extension_with_dot);
        layout_info_opt = octree::helpers::LayoutWithoutBase(std::move(default_layout_strategy), default_extension_with_dot);
    }

    disk::Layout layout(base_path, std::move(layout_info_opt->strategy), layout_info_opt->extension_with_dot);
    if (!create_index) {
        return Storage_<T, Codec>(RawStorage_<T, Codec>(std::move(layout)));
    }

    IndexMap map;
    helpers::update_index_map(map, layout);
    if (!map.empty()) {
        helpers::save_index_map(map, layout);
    }
    return Storage_<T, Codec>(RawStorage_<T, Codec>(std::move(layout)), std::move(map));
}

template <typename T, CodecFor<T> Codec>
IndexedStorage_<T, Codec> open_folder_indexed(
    const std::filesystem::path &base_path,
    OpenOptions options) {
    return IndexedStorage_<T, Codec>(open_folder<T, Codec>(base_path, true, std::move(options)));
}

} // namespace octree
