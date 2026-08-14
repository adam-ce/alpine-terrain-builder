#include "texture/dilate_colors.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "opencv_utils.h"

namespace texture {
namespace {

template <int n_channels>
struct Neighbourhood {
    cv::Vec<double, n_channels> sum;
    uint32_t count = 0;

    bool is_empty() const {
        return this->count == 0;
    }
};

template <int n_channels, typename Channel>
Neighbourhood<n_channels> sum_neighbours(const cv::Mat &image, const Mask &coverage, const int x, const int y) {
    using Texel = cv::Vec<Channel, n_channels>;

    Neighbourhood<n_channels> neighbourhood;
    for (int ny = std::max(y - 1, 0); ny <= std::min(y + 1, image.rows - 1); ny++) {
        for (int nx = std::max(x - 1, 0); nx <= std::min(x + 1, image.cols - 1); nx++) {
            if (!coverage.get(nx, ny)) {
                continue;
            }
            const Texel &neighbour = image.at<Texel>(ny, nx);
            for (int channel = 0; channel < n_channels; channel++) {
                neighbourhood.sum[channel] += neighbour[channel];
            }
            neighbourhood.count++;
        }
    }
    return neighbourhood;
}

template <int n_channels, typename Channel>
void dilate_round(cv::Mat &image, const Mask &coverage, Mask &grown) {
    using Texel = cv::Vec<Channel, n_channels>;

    for (int y = 0; y < image.rows; y++) {
        for (int x = 0; x < image.cols; x++) {
            if (coverage.get(x, y)) {
                continue;
            }

            const Neighbourhood<n_channels> neighbourhood =
                sum_neighbours<n_channels, Channel>(image, coverage, x, y);
            if (neighbourhood.is_empty()) {
                continue;
            }

            Texel &texel = image.at<Texel>(y, x);
            for (int channel = 0; channel < n_channels; channel++) {
                texel[channel] = cv::saturate_cast<Channel>(neighbourhood.sum[channel] / neighbourhood.count);
            }
            grown.set(x, y);
        }
    }
}

template <int n_channels, typename Channel>
void dilate_rounds(cv::Mat &image, Mask &coverage, const uint32_t radius) {
    for (uint32_t round = 0; round < radius; round++) {
        Mask grown = coverage.clone();
        dilate_round<n_channels, Channel>(image, coverage, grown);
        coverage = std::move(grown);
    }
}

} // namespace

void dilate_colors_inplace(cv::Mat &image, Mask &coverage, const uint32_t radius) {
    if (radius == 0) {
        return;
    }
    if (coverage.size() != glm::uvec2(image.cols, image.rows)) {
        throw std::invalid_argument("dilate_colors_inplace: coverage must have the size of the image");
    }

    visit_by_depth_and_channels(image.type(), [&]<int n_channels, typename Channel>() {
        dilate_rounds<n_channels, Channel>(image, coverage, radius);
    });
}

cv::Mat dilate_colors(const cv::Mat &image, const Mask &coverage, const uint32_t radius) {
    cv::Mat dilated = image.clone();
    Mask grown = coverage.clone();
    dilate_colors_inplace(dilated, grown, radius);
    return dilated;
}

} // namespace texture