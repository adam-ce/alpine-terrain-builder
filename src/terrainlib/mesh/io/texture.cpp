#include "mesh/io/texture.h"

namespace mesh {
namespace io {
namespace texture {

cv::Mat read_texture_from_encoded_bytes(std::span<const uint8_t> buffer) {
    cv::Mat raw_data = cv::Mat(1, buffer.size(), CV_8UC1, const_cast<uint8_t *>(buffer.data()));
    cv::Mat mat = cv::imdecode(raw_data, cv::IMREAD_UNCHANGED);
    mat.convertTo(mat, CV_8UC3);
    return mat;
}

void write_texture_to_encoded_buffer(const cv::Mat &image, std::vector<uint8_t> &buffer, const std::string& extension) {
    cv::Mat converted;
    image.convertTo(converted, CV_8UC3);
    cv::imencode(extension, image, buffer);
}
std::vector<uint8_t> write_texture_to_encoded_buffer(const cv::Mat &image, const std::string &extension) {
    std::vector<uint8_t> buffer;
    write_texture_to_encoded_buffer(image, buffer, extension);
    return buffer;
}

} // namespace texture
} // namespace io
} // namespace mesh
