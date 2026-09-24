#pragma once
#include <cstdint>
namespace rf_builder::tiles::jpeg {
double linear(std::uint8_t value);
std::uint8_t nonlinear(double value);
} // namespace rf_builder::tiles::jpeg
