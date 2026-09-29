#pragma once

#include "Error.h"
#include "io/envelope.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace rf_merger::statistics {

inline constexpr std::string_view file_name = "statistics.tmp";

enum class Category : std::uint8_t {
    LinkedLeft,
    LinkedRight,
    Mixed,
    LeftOnly,
    RightOnly,
    // Tiles restored from a recovery cache whose statistics were unavailable.
    Uncategorized,
};

struct Count {
    std::uint64_t tiles = 0;
    std::uint64_t bytes = 0;

    bool operator==(const Count&) const = default;
};

struct Totals {
    Count linked_left;
    Count linked_right;
    Count mixed;
    Count left_only;
    Count right_only;
    Count uncategorized;

    void add(Category category, std::uint64_t bytes);
    Count total() const;

    bool operator==(const Totals&) const = default;
};

using Schema = io::envelope::PayloadSchema<"rf_merger.Statistics", io::envelope::Version<1, Totals>>;

// Writes a temporary file and renames it into place, like the index checkpoint.
Expected<void> write(const Totals& totals, const std::filesystem::path& directory);
Expected<Totals> read(const std::filesystem::path& directory);

// Report lines with counts, percentages and byte sizes in one binary unit
// chosen from the total.
std::vector<std::string> format(const Totals& totals, bool complete);

} // namespace rf_merger::statistics
