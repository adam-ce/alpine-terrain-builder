#include "jpeg.h"
#include <algorithm>
#include <cmath>

namespace rf_builder::tiles::jpeg {
double linear(std::uint8_t value)
{
    const double encoded = value / 255.;
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}
std::uint8_t nonlinear(double value)
{
    value = std::clamp(value, 0., 1.);
    const double encoded = value <= 0.0031308 ? 12.92 * value : 1.055 * std::pow(value, 1. / 2.4) - 0.055;
    return std::uint8_t(std::clamp(std::lround(255 * encoded), 0L, 255L));
}
} // namespace rf_builder::tiles::jpeg
