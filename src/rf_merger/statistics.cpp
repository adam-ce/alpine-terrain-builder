#include "statistics.h"

#include <array>
#include <fmt/format.h>
#include <system_error>
#include <utility>

namespace rf_merger::statistics {

void Totals::add(Category category, std::uint64_t bytes)
{
    auto& count = [&]() -> Count& {
        switch (category) {
        case Category::LinkedLeft:
            return linked_left;
        case Category::LinkedRight:
            return linked_right;
        case Category::Mixed:
            return mixed;
        case Category::LeftOnly:
            return left_only;
        case Category::RightOnly:
            return right_only;
        case Category::Uncategorized:
            break;
        }
        return uncategorized;
    }();
    ++count.tiles;
    count.bytes += bytes;
}

Count Totals::total() const
{
    Count result;
    for (const auto* count : { &linked_left, &linked_right, &mixed, &left_only, &right_only, &uncategorized }) {
        result.tiles += count->tiles;
        result.bytes += count->bytes;
    }
    return result;
}

Expected<void> write(const Totals& totals, const std::filesystem::path& directory)
{
    const auto path = directory / file_name;
    auto temporary_path = path;
    temporary_path += ".tmp";
    if (auto written = io::envelope::write_to_path<Schema>(totals, temporary_path); !written) {
        return Error::propagate(std::move(written), "checkpoint RF merger statistics");
    }
    std::error_code error;
    std::filesystem::rename(temporary_path, path, error);
    if (error) {
        return Error::fail(Error::Code::Io, "replace RF merger statistics checkpoint", temporary_path, path, error);
    }
    return {};
}

Expected<Totals> read(const std::filesystem::path& directory) { return io::envelope::read_from_path<Schema>(directory / file_name); }

std::vector<std::string> format(const Totals& totals, bool complete)
{
    const auto total = totals.total();
    constexpr std::array<std::pair<const char*, double>, 3> units {
        { { "TiB", 1024. * 1024 * 1024 * 1024 }, { "GiB", 1024. * 1024 * 1024 }, { "MiB", 1024. * 1024 } }
    };
    auto unit = units.back();
    for (const auto& candidate : units) {
        if (double(total.bytes) >= candidate.second) {
            unit = candidate;
            break;
        }
    }
    const auto percent = [](std::uint64_t part, std::uint64_t whole) { return whole == 0 ? 0. : 100. * double(part) / double(whole); };
    const auto line = [&](std::string_view label, const Count& count) {
        return fmt::format("  {:<40} {:>10} tiles ({:5.1f}%), {:>10.2f} {} ({:5.1f}%)",
            label,
            count.tiles,
            percent(count.tiles, total.tiles),
            double(count.bytes) / unit.second,
            unit.first,
            percent(count.bytes, total.bytes));
    };
    std::vector<std::string> lines;
    lines.push_back(complete ? "RF merge statistics:" : "RF merge statistics (incomplete: recovery cache statistics were unavailable):");
    lines.push_back(line("hard-linked from left (shared with input)", totals.linked_left));
    lines.push_back(line("hard-linked from right (shared with input)", totals.linked_right));
    lines.push_back(line("new, pixels from both inputs", totals.mixed));
    lines.push_back(line("new, pixels only from left", totals.left_only));
    lines.push_back(line("new, pixels only from right", totals.right_only));
    if (totals.uncategorized.tiles != 0) {
        lines.push_back(line("restored without statistics", totals.uncategorized));
    }
    lines.push_back(line("total", total));
    return lines;
}

} // namespace rf_merger::statistics
