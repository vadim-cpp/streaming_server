#pragma once

#include <string>
#include <opencv2/core/mat.hpp>

class IAsciiConverter 
{
public:
    virtual ~IAsciiConverter() = default;
    virtual std::string convert(const cv::Mat& frame, int output_width, int output_height) = 0;
};