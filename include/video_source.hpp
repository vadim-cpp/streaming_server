#pragma once
#include <opencv2/opencv.hpp>
#include <memory>

class VideoSource 
{
public:
    VideoSource();
    ~VideoSource();
    bool is_available() const;
    cv::Mat capture_frame();
    void set_resolution(int width, int height);
    void close();
    
private:
    cv::VideoCapture cap_;
    int width_ = 640;
    int height_ = 480;
};