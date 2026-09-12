#include "jpeg.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace rf_builder::tiles::jpeg {
Expected<cv::Mat> decode(std::span<const std::uint8_t> bytes, unsigned side)
{
    // Validate SOF dimensions before OpenCV allocates a decoded raster. imdecode
    // otherwise allocates according to the untrusted header before returning.
    if (bytes.size() < 4 || bytes[0] != 0xff || bytes[1] != 0xd8 || bytes[bytes.size() - 2] != 0xff || bytes.back() != 0xd9) {
        return Error::fail(Error::Code::CorruptData, "source response is not a complete JPEG");
    }
    bool dimensions = false;
    for (std::size_t at = 2; at + 3 < bytes.size();) {
        if (bytes[at++] != 0xff) {
            break;
        }
        while (at < bytes.size() && bytes[at] == 0xff) {
            ++at;
        }
        if (at >= bytes.size()) {
            break;
        }
        const auto marker = bytes[at++];
        if (marker == 0xda || marker == 0xd9) {
            break;
        }
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) {
            continue;
        }
        if (at + 2 > bytes.size()) {
            break;
        }
        const std::size_t size = unsigned(bytes[at]) * 256 + bytes[at + 1];
        if (size < 2 || size > bytes.size() - at) {
            break;
        }
        if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
            if (size < 8 || bytes[at + 2] != 8 || unsigned(bytes[at + 3]) * 256 + bytes[at + 4] != side
                || unsigned(bytes[at + 5]) * 256 + bytes[at + 6] != side) {
                return Error::fail(Error::Code::CorruptData, "JPEG dimensions or sample precision disagree with provider configuration");
            }
            dimensions = true;
            break;
        }
        at += size;
    }
    if (!dimensions || bytes.size() > unsigned((std::numeric_limits<int>::max)())) {
        return Error::fail(Error::Code::CorruptData, "invalid JPEG header or response size");
    }
    try {
        const cv::Mat encoded(1, int(bytes.size()), CV_8UC1, const_cast<std::uint8_t*>(bytes.data()));
        auto image = cv::imdecode(encoded, cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
        if (image.empty() || image.type() != CV_8UC3 || image.rows != int(side) || image.cols != int(side)) {
            return Error::fail(Error::Code::CorruptData, "JPEG decode failed or returned unexpected dimensions");
        }
        cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
        return image;
    } catch (const cv::Exception& error) {
        return Error::fail(Error::Code::CorruptData, "JPEG decode: " + std::string(error.what()));
    }
}
std::string version()
{
    const auto& info = cv::getBuildInformation();
    const auto start = info.find("JPEG:");
    return std::string(CV_VERSION) + "/" + (start == std::string::npos ? "unknown JPEG" : info.substr(start, info.find('\n', start) - start));
}
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
