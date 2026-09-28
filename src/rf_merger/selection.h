#pragma once

#include "priorities.h"
#include <cstdint>
#include <optional>

namespace rf_merger::selection {

enum class Side : std::uint8_t {
    Left,
    Right,
};

// An original input tile competing for an output region.
struct Candidate {
    bool attributed;
    unsigned zoom_level;
};

// Whole-tile precedence: a tile with any nonzero attribution wins as a whole
// over a tile without attribution. Without attribution, prefer the finer tile,
// then the right input. Returns nothing when pixels must be compared.
inline std::optional<Side> whole_tile(const Candidate& left, const Candidate& right)
{
    if (left.attributed != right.attributed) {
        return left.attributed ? Side::Left : Side::Right;
    }
    if (left.attributed) {
        return std::nullopt;
    }
    return left.zoom_level > right.zoom_level ? Side::Left : Side::Right;
}

// Per-pixel comparison between two attributed tiles: prefer nonzero
// attribution, then priority rank, then higher original zoom, then the right
// input. Both zero attributions fall through to zoom and input order.
class Pixel {
public:
    Pixel(const priorities::Ranks& ranks, unsigned left_zoom, unsigned right_zoom)
        : m_ranks(&ranks)
        , m_left_zoom(left_zoom)
        , m_right_zoom(right_zoom)
    {
    }

    bool right_wins(std::uint16_t left, std::uint16_t right) const
    {
        const auto left_rank = (*m_ranks)[left];
        const auto right_rank = (*m_ranks)[right];
        if (left_rank != right_rank) {
            return right_rank > left_rank;
        }
        return m_right_zoom >= m_left_zoom;
    }

private:
    const priorities::Ranks* m_ranks;
    unsigned m_left_zoom;
    unsigned m_right_zoom;
};

} // namespace rf_merger::selection
