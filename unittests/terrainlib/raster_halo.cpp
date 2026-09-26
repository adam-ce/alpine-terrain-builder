#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "../temporary_directory.h"
#include "raster_store/read_tile_with_halo.h"

namespace {
using Tile = raster_store::Tile<float>;
using Id = radix::tile::Id;
using Resampling = raster::algorithm::Resampling;
struct MemoryCodec final : store::Codec<Tile> {
    mutable std::map<std::filesystem::path, Tile> tiles;
    mutable std::map<std::filesystem::path, unsigned> reads;
    std::vector<std::filesystem::path> paths(const std::filesystem::path& path) const override { return { path }; }
    Expected<Tile> read(const std::filesystem::path& path) const override
    {
        ++reads[path];
        if (!tiles.contains(path))
            return Error::fail(Error::Code::Io, "injected missing payload");
        return tiles.at(path);
    }
    Expected<void> write(const std::filesystem::path& path, const Tile& tile) const override
    {
        tiles.insert_or_assign(path, tile);
        return {};
    }
};
struct Fixture {
    test::TemporaryDirectory directory;
    MemoryCodec* codec;
    std::unique_ptr<raster_store::storage::IndexedStorage<float>> storage;
    raster_store::io::manifest::Metadata metadata { "zoom/x/y_google", "float32", "memory", 64, 64, 0 };
    Fixture()
    {
        auto owned = std::make_unique<MemoryCodec>();
        codec = owned.get();
        auto made = store::make_storage<raster_store::StoreTraits, Tile>(directory.path(),
            raster_store::io::manifest::index_format(),
            raster_store::path_layout::zoom_xy_google::zoom_x_y_google(),
            "float32",
            "memory",
            std::move(owned));
        REQUIRE(made);
        storage = std::make_unique<raster_store::storage::IndexedStorage<float>>(std::move(*made));
    }
    void add(Id id, float value, unsigned attribution = 1)
    {
        Tile tile(metadata.stored_tile_size);
        tile.data.fill(value);
        tile.source_attribution.fill(attribution);
        REQUIRE(storage->save(id, tile));
    }
    auto read(Id id, unsigned halo = 2, Resampling interpolation = Resampling::NearestNeighbourAndBox)
    {
        return raster_store::read_tile_with_halo(*storage, metadata, id, halo, interpolation);
    }
    void once() const
    {
        for (const auto& [path, count] : codec->reads) {
            INFO(path);
            CHECK(count == 1);
        }
    }
};
} // namespace

TEST_CASE("halo centre and missing coverage preserve payload bits", "[raster-halo]")
{
    Fixture f;
    const Id centre { 2, { 1, 1 } };
    f.add(centre, 42, 7);
    auto tile = f.read(centre);
    REQUIRE(tile);
    CHECK(tile->data.size() == glm::uvec2(68));
    CHECK(tile->data.pixel({ 0, 0 }) == 42);
    CHECK(tile->source_attribution.pixel({ 0, 0 }) == 0);
    CHECK(tile->source_attribution.pixel({ 2, 2 }) == 7);
    f.once();
    CHECK_FALSE(f.read({ 2, { 4, 0 } }));
    CHECK_FALSE(f.read({ 2, { 0, 0 } }));
    CHECK_FALSE(f.read({ 0, { 0, 0 } }));
    CHECK_FALSE(f.read(centre, 65));
}

TEST_CASE("halo neighbours supply every edge and corner including zero attribution", "[raster-halo]")
{
    Fixture f;
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < 3; ++x)
            f.add({ 2, { x, y } }, float(10 * y + x), 0);
    auto tile = f.read({ 2, { 1, 1 } }, 64);
    REQUIRE(tile);
    for (unsigned y = 0; y < 192; ++y)
        for (unsigned x = 0; x < 192; ++x) {
            CHECK(tile->data.pixel({ x, y }) == float(10 * (y / 64) + x / 64));
            CHECK(tile->source_attribution.pixel({ x, y }) == 0);
        }
    f.once();
}

TEST_CASE("halo descendants stop at physical tiles and ancestors fill only uncovered regions", "[raster-halo]")
{
    Fixture f;
    const Id root { 0, { 0, 0 } }, centre { 2, { 1, 1 } };
    f.add(root, 5, 5);
    f.add(centre, 10, 10);
    f.add({ 3, { 4, 2 } }, 20, 0);
    f.add({ 4, { 8, 4 } }, 90, 9); // Physical inner at zoom 3 must win.
    auto tile = f.read(centre, 64);
    REQUIRE(tile);
    CHECK(tile->data.pixel({ 128, 64 }) == 20);
    CHECK(tile->source_attribution.pixel({ 128, 64 }) == 0);
    CHECK(tile->data.pixel({ 160, 64 }) == 5);
    CHECK(tile->data.pixel({ 128, 96 }) == 5);
    CHECK(f.codec->reads.size() == 3);
    f.once();
}

TEST_CASE("halo descent cutoff uses an ancestor without loading deep payloads", "[raster-halo]")
{
    Fixture f;
    f.add({ 0, { 0, 0 } }, 7);
    f.add({ 2, { 1, 1 } }, 11);
    f.add({ 7, { 64, 32 } }, 99);
    auto tile = f.read({ 2, { 1, 1 } }, 64);
    REQUIRE(tile);
    CHECK(tile->data.pixel({ 128, 64 }) == 7);
    CHECK(f.codec->reads.size() == 2);
    f.once();
}

TEST_CASE("halo root wraps horizontally and replicates beyond vertical limits", "[raster-halo]")
{
    Fixture f;
    f.add({ 0, { 0, 0 } }, 12, 4);
    auto tile = f.read({ 0, { 0, 0 } });
    REQUIRE(tile);
    CHECK(tile->source_attribution.pixel({ 0, 2 }) == 4);
    CHECK(tile->source_attribution.pixel({ 0, 0 }) == 0);
    CHECK(tile->source_attribution.pixel({ 2, 67 }) == 0);
    f.once();
}

TEST_CASE("stored halo crops exactly without fetching neighbours or validating numerical mapping", "[raster-halo]")
{
    Fixture f;
    f.metadata.halo_width = 3;
    f.metadata.stored_tile_size = 70;
    f.metadata.value_mapping = raster_store::pixel::Mapping::SRGBA;
    f.add({ 1, { 0, 0 } }, std::bit_cast<float>(0x7fc01234u), 0);
    f.add({ 1, { 1, 0 } }, 1);
    for (unsigned halo : { 0u, 1u, 3u }) {
        f.codec->reads.clear();
        auto tile = f.read({ 1, { 0, 0 } }, halo);
        REQUIRE(tile);
        CHECK(tile->data.size() == glm::uvec2(64 + 2 * halo));
        CHECK(std::bit_cast<unsigned>(tile->data.pixel({ 0, 0 })) == 0x7fc01234u);
        CHECK(f.codec->reads.size() == 1);
        f.once();
    }
    CHECK_FALSE(f.read({ 1, { 0, 0 } }, 4));
}

TEST_CASE("halo propagates indexed payload errors", "[raster-halo]")
{
    Fixture f;
    f.add({ 1, { 0, 0 } }, 1);
    f.add({ 1, { 1, 0 } }, 2);
    f.codec->tiles.erase(f.directory.path() / "1/1/0");
    auto result = f.read({ 1, { 0, 0 } });
    REQUIRE_FALSE(result);
    CHECK(result.error().code() == Error::Code::Io);
}

TEST_CASE("halo distant ancestor samples use bounded windows with every resampling method", "[raster-halo]")
{
    for (auto interpolation :
        { Resampling::NearestNeighbourAndBox, Resampling::BiliinearAndBox, Resampling::Lanczos2, Resampling::Lanczos3, Resampling::Lanczos4 }) {
        Fixture f;
        f.add({ 0, { 0, 0 } }, 23, 3);
        f.add({ 20, { 400000, 500000 } }, 42, 4);
        auto result = f.read({ 20, { 400000, 500000 } }, 2, interpolation);
        REQUIRE(result);
        CHECK(result->data.pixel({ 0, 0 }) == Catch::Approx(23).margin(1e-4));
        CHECK(result->source_attribution.pixel({ 0, 0 }) == 3);
        CHECK(result->data.pixel({ 2, 2 }) == 42);
        f.once();
    }
}

TEST_CASE("halo ancestor windows match complete-ancestor scaling at edges and cutouts", "[raster-halo]")
{
    for (auto interpolation :
        { Resampling::NearestNeighbourAndBox, Resampling::BiliinearAndBox, Resampling::Lanczos2, Resampling::Lanczos3, Resampling::Lanczos4 }) {
        Fixture f;
        Tile ancestor(64);
        for (unsigned y = 0; y < 64; ++y)
            for (unsigned x = 0; x < 64; ++x) {
                ancestor.data.pixel({ x, y }) = float(x + 100 * y);
                ancestor.source_attribution.pixel({ x, y }) = (x + 2 * y) % 7;
            }
        REQUIRE(f.storage->save({ 0, { 0, 0 } }, ancestor));
        const Id centre { 2, { 0, 1 } };
        f.add(centre, -1, 9);
        auto tile = f.read(centre, 64, interpolation);
        REQUIRE(tile);
        const unsigned support = *raster::algorithm::required_halo(2, interpolation);
        const auto padded = *raster::make_clamped_view(ancestor.data, glm::ivec2(-int(support)), glm::uvec2(64 + 2 * support));
        const auto padded_attribution = *raster::make_clamped_view(ancestor.source_attribution, glm::ivec2(-int(support)), glm::uvec2(64 + 2 * support));
        auto full = raster_store::scaler::scale(padded, padded_attribution, support, 2, interpolation, raster_store::pixel::Mapping::Linear);
        REQUIRE(full);
        for (unsigned y = 0; y < 192; ++y)
            for (unsigned x = 0; x < 192; ++x) {
                if (x >= 64 && x < 128 && y >= 64 && y < 128)
                    continue;
                const glm::uvec2 position((x + 192) % 256, y);
                CHECK(tile->data.pixel({ x, y }) == full->first.pixel(position));
                CHECK(tile->source_attribution.pixel({ x, y }) == full->second.pixel(position));
            }
        f.once();
    }
}

TEST_CASE("halo box reductions use physical zero-attribution samples", "[raster-halo]")
{
    Fixture f;
    f.add({ 2, { 1, 1 } }, -1);
    Tile finer(64);
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x)
            finer.data.pixel({ x, y }) = float(x + 100 * y);
    REQUIRE(f.storage->save({ 3, { 4, 2 } }, finer));
    auto tile = f.read({ 2, { 1, 1 } }, 64);
    REQUIRE(tile);
    CHECK(tile->data.pixel({ 128, 64 }) == 50.5f);
    CHECK(tile->data.pixel({ 159, 95 }) == 6312.5f);
    CHECK(tile->source_attribution.pixel({ 128, 64 }) == 0);
    CHECK(tile->data.pixel({ 160, 64 }) == -1);
    CHECK(tile->source_attribution.pixel({ 160, 64 }) == 0);
    f.once();
}

TEST_CASE("halo exact centre and replication preserve nonfinite and signed-zero bits", "[raster-halo]")
{
    Fixture f;
    Tile centre(64);
    centre.data.fill(std::bit_cast<float>(0x7fc01234u));
    centre.data.pixel({ 63, 63 }) = -0.f;
    REQUIRE(f.storage->save({ 2, { 1, 1 } }, centre));
    auto tile = f.read({ 2, { 1, 1 } }, 2);
    REQUIRE(tile);
    CHECK(std::bit_cast<unsigned>(tile->data.pixel({ 0, 0 })) == 0x7fc01234u);
    CHECK(std::bit_cast<unsigned>(tile->data.pixel({ 2, 2 })) == 0x7fc01234u);
    CHECK(std::bit_cast<unsigned>(tile->data.pixel({ 67, 67 })) == 0x80000000u);
    auto exact = f.read({ 2, { 1, 1 } }, 0);
    REQUIRE(exact);
    CHECK(std::ranges::equal(exact->data.bytes(), centre.data.bytes()));
}

TEST_CASE("halo nonconstant distant ancestor preserves fractional phase", "[raster-halo]")
{
    Fixture f;
    Tile ancestor(64);
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x)
            ancestor.data.pixel({ x, y }) = float(x);
    REQUIRE(f.storage->save({ 0, { 0, 0 } }, ancestor));
    const Id centre { 20, { 400000, 500000 } };
    f.add(centre, -1);
    auto tile = f.read(centre, 2, Resampling::BiliinearAndBox);
    REQUIRE(tile);
    // The left halo begins two requested pixels before the centre's boundary.
    const double expected = (400000.0 * 64 - 2 + 0.5) / double(1u << 20) - 0.5;
    CHECK(std::abs(tile->data.pixel({ 0, 2 }) - expected) < 0.00001);
    f.once();
}

TEST_CASE("halo ancestor fallback supports a zoom gap of 30 without rebasing", "[raster-halo]")
{
    for (auto method : { Resampling::NearestNeighbourAndBox, Resampling::BiliinearAndBox, Resampling::Lanczos3 }) {
        Fixture f;
        f.add({ 0, { 0, 0 } }, 23, 3);
        const Id centre { 30, { (1u << 24) - 1, (1u << 24) - 1 } };
        f.add(centre, 42, 4);
        auto result = f.read(centre, 2, method);
        REQUIRE(result);
        CHECK(result->data.pixel({ 0, 0 }) == Catch::Approx(23).margin(1e-4));
        CHECK(result->data.pixel({ 67, 32 }) == Catch::Approx(23).margin(1e-4));
        CHECK(result->source_attribution.pixel({ 67, 32 }) == 3);
        f.once();
    }
}

TEST_CASE("halo ancestor fallback rejects zoom gaps above 30", "[raster-halo]")
{
    for (unsigned gap : { 31u, 32u }) {
        Fixture f;
        f.add({ 0, { 0, 0 } }, 23, 3);
        const Id centre { gap, { 1u << 24, 1u << 24 } };
        f.add(centre, 42, 4);
        auto result = f.read(centre, 2, Resampling::Lanczos3);
        REQUIRE_FALSE(result);
        CHECK(result.error().code() == Error::Code::InvalidInput);
        CHECK(result.error().to_string().find("ancestor fallback zoom gap") != std::string::npos);
    }
}
