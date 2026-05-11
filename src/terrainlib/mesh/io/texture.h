#pragma once

#include <string_view>
#include <vector>
#include <span>

#include <opencv2/opencv.hpp>

namespace mesh::io {

struct ImageAndExt {
    cv::Mat image;
    std::string ext;
};

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer);
void write_texture_to_encoded_buffer(const ImageAndExt& item, std::vector<uint8_t> &buffer);
void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string &extension);
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string &extension);
std::vector<uint8_t> write_texture_to_encoded_buffer(const ImageAndExt &item);

}

#include <zpp_bits.h>

namespace zpp::bits {

template <typename Archive>
auto serialize(Archive &archive, const mesh::io::ImageAndExt &item) {
    const std::vector<uint8_t> encoded = mesh::io::write_texture_to_encoded_buffer(item.image, item.ext);
    return archive(item.ext, encoded);
}

template <typename Archive>
auto serialize(Archive &archive, mesh::io::ImageAndExt &item) {
    std::vector<uint8_t> encoded;

    auto result = archive(item.ext, encoded);
    if (failure(result)) {
        return result;
    }

    item.image = mesh::io::read_texture_from_encoded_bytes(encoded);
    return result;
}

} // namespace zpp::bits
