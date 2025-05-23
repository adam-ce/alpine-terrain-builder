namespace {
// from https://github.com/boostorg/container_hash/blob/ee5285bfa64843a11e29700298c83a37e3132fcd/include/boost/container_hash/detail/hash_mix.hpp#L17
template <std::size_t Bits>
struct hash_mix_impl;

template <>
struct hash_mix_impl<64> {
    inline static std::uint64_t fn(std::uint64_t x) {
        std::uint64_t const m = 0xe9846af9b1a615d;

        x ^= x >> 32;
        x *= m;
        x ^= x >> 32;
        x *= m;
        x ^= x >> 28;

        return x;
    }
};

template <>
struct hash_mix_impl<32> {
    inline static std::uint32_t fn(std::uint32_t x) {
        std::uint32_t const m1 = 0x21f0aaad;
        std::uint32_t const m2 = 0x735a2d97;

        x ^= x >> 16;
        x *= m1;
        x ^= x >> 15;
        x *= m2;
        x ^= x >> 15;

        return x;
    }
};

inline std::size_t hash_mix(std::size_t v) {
    return hash_mix_impl<sizeof(std::size_t) * CHAR_BIT>::fn(v);
}

// from https://github.com/boostorg/container_hash/blob/ee5285bfa64843a11e29700298c83a37e3132fcd/include/boost/container_hash/hash.hpp#L468
template <class T>
inline void hash_combine_core(std::size_t &seed, const T &v) {
    seed = hash_mix(seed + 0x9e3779b9 + std::hash<T>()(v));
}

// modified from https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/57595105#57595105
template <typename T, typename... Rest>
void hash_combine_core(std::size_t &seed, const T &v, const Rest &...rest) {
    seed = hash_mix(seed, v);
    (hash_combine_core(seed, rest), ...);
}
}

template <typename... Rest>
size_t hash_combine(const Rest &...rest) {
    size_t h;
    (hash_combine_core(h, rest), ...);
    return h;
}
