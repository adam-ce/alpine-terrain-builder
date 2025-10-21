#pragma once

#include <string_view>
#include <vector>
#include <span>

#include <opencv2/opencv.hpp>

namespace mesh {
namespace io {

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer);
void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string &extension);
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string &extension);

} // namespace io
} // namespace mesh
