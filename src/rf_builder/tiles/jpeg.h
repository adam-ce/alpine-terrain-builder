#pragma once
#include "Error.h"
#include <cstdint>
#include <opencv2/core.hpp>
#include <span>
#include <string>
namespace rf_builder::tiles::jpeg {
Expected<cv::Mat> decode(std::span<const std::uint8_t> bytes, unsigned side);
std::string version();
double linear(std::uint8_t value);
std::uint8_t nonlinear(double value);
} // namespace rf_builder::tiles::jpeg
