#pragma once

#include <compare>

struct StrongDouble {
    double value;

    std::strong_ordering operator<=>(const StrongDouble &other) const {
        return std::strong_order(value, other.value);
    }

    bool operator==(const StrongDouble &other) const {
        return std::strong_order(value, other.value) == 0;
    }
};
