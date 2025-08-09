#pragma once

#include "video_source_interface.hpp"
#include <opencv2/opencv.hpp>
#include <memory>

class VideoSource : public IVideoSource 
{
public:
    VideoSource();
    ~VideoSource();
    bool is_available() const override;
    cv::Mat capture_frame() override;
    void set_resolution(int width, int height) override;
    void close() override;
    
private:
    cv::VideoCapture cap_;
    int width_ = 640;
    int height_ = 480;
};