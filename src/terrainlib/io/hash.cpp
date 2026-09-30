#include "io/hash.h"

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <libassert/assert.hpp>

namespace io::hash {
namespace {

    std::vector<std::byte> xxh3_64(const std::span<const std::byte> bytes)
    {
        XXH64_canonical_t canonical;
        XXH64_canonicalFromHash(&canonical, XXH3_64bits(bytes.data(), bytes.size()));
        const auto* digest = reinterpret_cast<const std::byte*>(canonical.digest);
        return { digest, digest + sizeof(canonical.digest) };
    }

} // namespace

std::vector<std::byte> data(const std::span<const std::byte> bytes, const Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::None:
        return {};
    case Algorithm::Xxh3_64:
        return xxh3_64(bytes);
    }
    PANIC("unsupported hash algorithm", static_cast<unsigned>(algorithm));
}

} // namespace io::hash
