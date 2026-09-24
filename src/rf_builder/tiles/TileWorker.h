#pragma once
#include "HttpClient.h"
#include "Mask.h"
#include "inputs.h"
#include "planning.h"
#include "raster/View.h"
#include <list>
#include <memory>
#include <radix/raster.h>
#include <unordered_map>

namespace rf_builder::tiles {
class TileWorker {
public:
    static Expected<std::unique_ptr<TileWorker>> open(const inputs::Record& record,
        const planning::Coverage& coverage,
        NetworkCounters& counters,
        std::size_t cache_bytes = 64 * 1024 * 1024,
        RetryPolicy retry = {});
    Expected<run::Prepared<glm::u8vec3>> prepare(const run::Key& key);
    std::size_t retained_bytes() const { return m_retained_bytes; }

private:
    using Image = std::shared_ptr<const radix::Raster<glm::u8vec3>>;
    struct Entry {
        run::Key key;
        Image image;
        std::size_t bytes;
    };
    struct Supplier {
        run::Key key;
        Image image;
    };
    TileWorker(
        const inputs::Record& record, const planning::Coverage& coverage, NetworkCounters& counters, Mask mask, std::size_t cache_bytes, RetryPolicy retry);
    Expected<Image> image(const run::Key& key);
    Expected<std::optional<Supplier>> available(const run::Key& key);
    Expected<bool> finer(const run::Key& source, const run::Key& candidate, unsigned matching_zoom);
    Expected<glm::u8vec3> pixel(unsigned zoom, std::int64_t x, std::int64_t y, const Supplier& edge);
    Expected<void> sample(const Supplier& supplier, unsigned zoom, glm::u64vec2 origin, const raster::View<glm::u8vec3>& destination);
    Expected<void> assemble(
        raster_store::Tile<glm::u8vec3>& tile, std::vector<std::uint8_t>& valid, const run::Key& candidate, const run::Key& source, const Supplier& supplier);
    inputs::Record m_record;
    const planning::Coverage& m_coverage;
    Mask m_mask;
    HttpClient m_http;
    unsigned m_offset;
    std::size_t m_cache_bytes;
    std::size_t m_retained_bytes = 0;
    std::list<Entry> m_cache;
    std::unordered_map<run::Key, std::list<Entry>::iterator, run::Key::Hasher> m_lookup;
};
} // namespace rf_builder::tiles
