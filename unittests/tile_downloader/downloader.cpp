#include "TileDownloader.h"

#include <filesystem>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

class TemporaryPyramid {
public:
    explicit TemporaryPyramid(std::string_view name)
        : _path(std::filesystem::temp_directory_path() / name) {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
        std::filesystem::create_directories(_path);
    }

    ~TemporaryPyramid() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    [[nodiscard]] const std::filesystem::path &path() const {
        return _path;
    }

    [[nodiscard]] std::filesystem::path tile_path(const radix::tile::Id &tile) const {
        return google_tile_path(_path, tile, ".jpeg");
    }

    void create_pending(const radix::tile::Id &tile) const {
        const auto path = tile_path(tile);
        std::filesystem::create_directories(path.parent_path());
        write_file_children_pending(path, std::vector<char>{'t', 'i', 'l', 'e'});
    }

    void create_complete(const radix::tile::Id &tile) const {
        create_pending(tile);
        mark_tile_children_complete(
            tile_path(tile), std::filesystem::file_time_type::clock::now());
    }

private:
    std::filesystem::path _path;
};

const TileUrlBuilder missing_file_url({
    "file:///definitely-missing-atb-tile/{zoom}/{x}/{y}.jpeg",
    TileYDirection::Down
});

}

TEST_CASE("tile downloader promotes parents after completed children")
{
    const TemporaryPyramid pyramid("atb-downloader-complete-pyramid");
    const radix::tile::Id root{0, {0, 0}};
    const auto children = root.children();

    pyramid.create_pending(root);
    for (const auto &child : children) {
        pyramid.create_pending(child);
    }

    TileDownloader downloader(missing_file_url, pyramid.path(), 1u, root.zoom_level);
    REQUIRE(downloader.download_recursive(root));

    REQUIRE(std::filesystem::exists(pyramid.tile_path(root)));
    CHECK_FALSE(std::filesystem::exists(children_pending_tile_path(pyramid.tile_path(root))));
    const auto root_time = std::filesystem::last_write_time(pyramid.tile_path(root));

    for (const auto &child : children) {
        CHECK(std::filesystem::exists(pyramid.tile_path(child)));
        CHECK_FALSE(std::filesystem::exists(children_pending_tile_path(pyramid.tile_path(child))));
        CHECK(std::filesystem::last_write_time(pyramid.tile_path(child)) < root_time);
    }

    REQUIRE(std::filesystem::remove(pyramid.tile_path(children.front())));
    TileDownloader resumed_downloader(missing_file_url, pyramid.path(), 1u, root.zoom_level);
    CHECK(resumed_downloader.download_recursive(root));
    CHECK_FALSE(std::filesystem::exists(pyramid.tile_path(children.front())));
}

TEST_CASE("tile downloader leaves ancestors pending after a child failure")
{
    const TemporaryPyramid pyramid("atb-downloader-failed-pyramid");
    const radix::tile::Id root{0, {0, 0}};
    const auto children = root.children();

    pyramid.create_pending(root);
    for (size_t i = 1; i < children.size(); ++i) {
        pyramid.create_complete(children[i]);
    }

    TileDownloader downloader(missing_file_url, pyramid.path(), 1u, root.zoom_level);
    CHECK_FALSE(downloader.download_recursive(root));

    CHECK_FALSE(std::filesystem::exists(pyramid.tile_path(root)));
    CHECK(std::filesystem::exists(children_pending_tile_path(pyramid.tile_path(root))));
    for (size_t i = 1; i < children.size(); ++i) {
        CHECK(std::filesystem::exists(pyramid.tile_path(children[i])));
    }
}
