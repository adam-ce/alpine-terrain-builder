#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace io::hash {

enum class Algorithm : std::uint8_t {
    None,
    Xxh3_64,
};

// Returns no bytes for None and the 8 bytes of XXH3-64 with seed 0 in big-endian order, the canonical
// xxHash form, for Xxh3_64. The algorithm must be an enumerator.
std::vector<std::byte> data(std::span<const std::byte> bytes, Algorithm algorithm);

} // namespace io::hash
