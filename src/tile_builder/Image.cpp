#include <opencv2/opencv.hpp>
#include "Image.h"
#include <string>

void image::saveImageAsPng(const Image<glm::u8vec3>& inputImage, const std::string& path)
{
    const int width = static_cast<int>(inputImage.width());
    const int height = static_cast<int>(inputImage.height());

    cv::Mat image(height, width, CV_8UC3);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const glm::u8vec3& pixel = inputImage.pixel(height - row - 1, col);  // Flip vertically
            image.at<cv::Vec3b>(row, col) = cv::Vec3b(pixel.z, pixel.y, pixel.x); // RGB to BGR
        }
    }

    cv::imwrite(path, image);
}
