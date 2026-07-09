#pragma once

namespace hash {

namespace {
// from https://github.com/boostorg/container_hash/blob/ee5285bfa64843a11e29700298c83a37e3132fcd/include/boost/container_hash/detail/hash_mix.hpp#L17
template <size_t Bits>
struct mix_impl;

template <>
struct mix_impl<64> {
    inline static uint64_t fn(uint64_t x) {
        uint64_t const m = 0xe9846af9b1a615d;

        x ^= x >> 32;
        x *= m;
        x ^= x >> 32;
        x *= m;
        x ^= x >> 28;

        return x;
    }
};

template <>
struct mix_impl<32> {
    inline static uint32_t fn(uint32_t x) {
        uint32_t const m1 = 0x21f0aaad;
        uint32_t const m2 = 0x735a2d97;

        x ^= x >> 16;
        x *= m1;
        x ^= x >> 15;
        x *= m2;
        x ^= x >> 15;

        return x;
    }
};

inline size_t mix(size_t v) {
    return mix_impl<sizeof(size_t) * CHAR_BIT>::fn(v);
}
}

constexpr size_t default_seed() {
    return 0x9e3779b9;
}

// from https://github.com/boostorg/container_hash/blob/ee5285bfa64843a11e29700298c83a37e3132fcd/include/boost/container_hash/hash.hpp#L468
template <class T>
inline void append(size_t &seed, const T &v) {
    seed = mix(seed + 0x9e3779b9 + std::hash<T>()(v));
}

// modified from https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/57595105#57595105
template <typename T, typename... Rest>
void append(size_t &seed, const T &v, const Rest &...rest) {
    seed = mix(seed, v);
    (append(seed, rest), ...);
}

template <typename... Rest>
size_t combine(const Rest &...rest) {
    size_t h = default_seed();
    (append(h, rest), ...);
    return h;
}

}
