#include "ascii_converter.hpp"

#include <opencv2/opencv.hpp>

AsciiConverter::AsciiConverter() {}

cv::Mat AsciiConverter::resize_frame(const cv::Mat& frame, int width, int height) 
{
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(width, height));
    return resized;
}

cv::Mat AsciiConverter::convert_to_grayscale(const cv::Mat& frame) 
{
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

std::string AsciiConverter::convert(const cv::Mat& frame, int output_width, int output_height) 
{
    if (frame.empty()) return "";
    
    // Обработка кадра
    cv::Mat processed = convert_to_grayscale(frame);
    processed = resize_frame(processed, output_width, output_height);
    
    // Генерация ASCII
    std::string ascii_frame;
    for (int y = 0; y < processed.rows; y++) 
    {
        for (int x = 0; x < processed.cols; x++) {
            uchar pixel = processed.at<uchar>(y, x);
            int index = pixel * (ascii_chars_.size() - 1) / 255;
            ascii_frame += ascii_chars_[index];
        }
        ascii_frame += '\n';
    }
    return ascii_frame;
}