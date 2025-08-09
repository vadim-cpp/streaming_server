#pragma once

#include "ascii_converter_interface.hpp"
#include <string>
#include <opencv2/core/mat.hpp>

class AsciiConverter : public IAsciiConverter 
{
public:
    AsciiConverter();
    std::string convert(const cv::Mat& frame, int output_width, int output_height) override;
    
private:
    const std::string ascii_chars_ = "@%#*+=-:. ";
    cv::Mat resize_frame(const cv::Mat& frame, int width, int height);
    cv::Mat convert_to_grayscale(const cv::Mat& frame);
};