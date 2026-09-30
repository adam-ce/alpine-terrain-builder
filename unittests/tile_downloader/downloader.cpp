/*****************************************************************************
 * AlpineMaps.org
 * Copyright (C) 2026 Adam Celarek-Litofcenko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include "TileDownloader.h"

#include <filesystem>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

class TemporaryPyramid {
public:
    explicit TemporaryPyramid(std::string_view name)
        : m_path(std::filesystem::temp_directory_path() / name)
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
        std::filesystem::create_directories(m_path);
    }

    ~TemporaryPyramid()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

    [[nodiscard]] std::filesystem::path tile_path(const radix::tile::Id& tile) const { return google_tile_path(m_path, tile, ".jpeg"); }

    void create_pending(const radix::tile::Id& tile) const
    {
        const auto path = tile_path(tile);
        std::filesystem::create_directories(path.parent_path());
        write_file_children_pending(path, std::vector<char> { 't', 'i', 'l', 'e' });
    }

    void create_complete(const radix::tile::Id& tile) const
    {
        create_pending(tile);
        mark_tile_children_complete(tile_path(tile));
    }

private:
    std::filesystem::path m_path;
};

const TileUrlBuilder missing_file_url({ "file:///definitely-missing-atb-tile/{zoom}/{x}/{y}.jpeg", TileYDirection::Down });

} // namespace

TEST_CASE("tile downloader promotes parents after completed children")
{
    const TemporaryPyramid pyramid("atb-downloader-complete-pyramid");
    const radix::tile::Id root { 0, { 0, 0 } };
    const auto children = root.children();

    pyramid.create_pending(root);
    for (const auto& child : children) {
        pyramid.create_pending(child);
    }

    TileDownloader downloader(missing_file_url, pyramid.path(), 1u, root.zoom_level);
    REQUIRE(downloader.download_recursive(root));

    REQUIRE(std::filesystem::exists(pyramid.tile_path(root)));
    CHECK_FALSE(std::filesystem::exists(children_pending_tile_path(pyramid.tile_path(root))));

    for (const auto& child : children) {
        CHECK(std::filesystem::exists(pyramid.tile_path(child)));
        CHECK_FALSE(std::filesystem::exists(children_pending_tile_path(pyramid.tile_path(child))));
    }

    REQUIRE(std::filesystem::remove(pyramid.tile_path(children.front())));
    TileDownloader resumed_downloader(missing_file_url, pyramid.path(), 1u, root.zoom_level);
    CHECK(resumed_downloader.download_recursive(root));
    CHECK_FALSE(std::filesystem::exists(pyramid.tile_path(children.front())));
}

TEST_CASE("tile downloader leaves ancestors pending after a child failure")
{
    const TemporaryPyramid pyramid("atb-downloader-failed-pyramid");
    const radix::tile::Id root { 0, { 0, 0 } };
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
