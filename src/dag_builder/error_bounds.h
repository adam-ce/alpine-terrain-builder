#pragma once

#include <cstdint>
#include <array>

constexpr double EARTH_CIRCUMFERENCE_M = 40075016.686; // Earth's circumference in metres
constexpr uint32_t MAX_LEVEL = 30;

// Compute metres per pixel at equator for a given zoom level
constexpr double metres_per_pixel(uint32_t zoom) {
    return EARTH_CIRCUMFERENCE_M / (256.0 * (1ULL << zoom));
}

constexpr std::array<double, MAX_LEVEL> generate_meters_per_pixel_array() {
    std::array<double, MAX_LEVEL> arr = {};
    for (uint32_t zoom = 0; zoom < MAX_LEVEL; zoom++) {
        arr[zoom] = metres_per_pixel(zoom);
    }
    return arr;
}

constexpr std::array<double, MAX_LEVEL> METERS_PER_PIXEL_AT_EQUATOR = generate_meters_per_pixel_array();

static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[0] - 156543) < 1, "level 0 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[10] - 152.874) < 0.001, "level 10 mismatch");
static_assert(std::abs(METERS_PER_PIXEL_AT_EQUATOR[20] - 0.149) < 0.001, "level 20 mismatch");
