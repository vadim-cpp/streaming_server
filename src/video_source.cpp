#include "video_source.hpp"

VideoSource::VideoSource() 
{
    cap_.open(0);
    if (!cap_.isOpened()) {
        throw std::runtime_error("Could not open video source");
    }
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
}

VideoSource::~VideoSource() 
{
    close();
}

void VideoSource::close()
{
    if (cap_.isOpened()) 
    {
        cap_.release();
    }
}

bool VideoSource::is_available() const 
{
    return cap_.isOpened();
}

cv::Mat VideoSource::capture_frame() 
{
    cv::Mat frame;
    cap_ >> frame;
    return frame;
}

void VideoSource::set_resolution(int width, int height) 
{
    width_ = width;
    height_ = height;
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
}