#pragma once

#include <opencv2/opencv.hpp>

class IVideoSource 
{
public:
    virtual ~IVideoSource() = default;
    virtual bool is_available() const = 0;
    virtual cv::Mat capture_frame() = 0;
    virtual void set_resolution(int width, int height) = 0;
    virtual void open(int index) = 0;
    virtual void close() = 0;
};