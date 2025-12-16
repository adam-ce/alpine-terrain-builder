#include <cassert>
#include <iostream>
#include <stack>
#include <vector>
#include <array>

#include "FixedVector.h"

namespace {
uint32_t prev_power_of_two(uint32_t x) {
    if (x == 0) {
        return 0;
    }
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}
}

class ThreadLocalPool {
private:
    static constexpr size_t MIN_SIZE_POW_2 = 4;  // 2^4 = 16 bytes minimum
    static constexpr size_t MAX_SIZE_POW_2 = 31; // 2^32 bytes maximum
    static constexpr size_t MIN_SIZE = 1ULL << MIN_SIZE_POW_2;
    static constexpr size_t MAX_SIZE = 1ULL << MAX_SIZE_POW_2;
    static constexpr size_t NUM_BUCKETS = MAX_SIZE_POW_2 - MIN_SIZE_POW_2 + 1;
    static constexpr size_t BUCKET_SIZE = 10;

    using Allocation = void*;
    using Bucket = FixedVector<Allocation, BUCKET_SIZE>;

public:
    static Allocation allocate(size_t size) {
        size_t index = size_to_index(size);
        auto& bucket = pools()[index];
        if (!bucket.empty()) {
            auto alloc = *bucket.begin();
            bucket.pop_back();
            return alloc;
        }
        return ::operator new(1ULL << index);
    }

    static void deallocate(Allocation alloc, size_t size) {
        size_t index = size_to_index(size);
        auto& bucket = pools()[index];
        if (!bucket.full()) {
            bucket.push_back(alloc);
        } else {
            ::operator delete(alloc);
        }
    }

    static thread_local std::array<Bucket, NUM_BUCKETS> &pools() {
        static thread_local std::array<Bucket, NUM_BUCKETS> p;
        return p;
    }

    static size_t size_to_index(size_t size) {
        if (size < MIN_SIZE) {
            // small allocations all go to bucket 0
            return 0;
        }
        if (size > MAX_SIZE) {
            // huge allocation bucket
            return NUM_BUCKETS - 1;
        }

        return 31 - __builtin_clz(static_cast<uint32_t>(size - 1)) - MIN_SIZE_POW_2 + 1;
    }
};

template <typename T>
class PoolAllocator {
public:
    using value_type = T;
    PoolAllocator() noexcept = default;
    template <class U>
    PoolAllocator(const PoolAllocator<U> &) noexcept {}

    T *allocate(std::size_t n) {
        return static_cast<T *>(ThreadLocalPool::allocate(n * sizeof(T)));
    }

    void deallocate(T *ptr, std::size_t n) noexcept {
        ThreadLocalPool::deallocate(ptr, n * sizeof(T));
    }
};

template <typename T, typename U>
bool operator==(const PoolAllocator<T> &, const PoolAllocator<U> &) {
    return true;
}
template <typename T, typename U>
bool operator!=(const PoolAllocator<T> &, const PoolAllocator<U> &) {
    return false;
}
