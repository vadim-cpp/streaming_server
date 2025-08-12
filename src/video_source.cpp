#include "video_source.hpp"
#include "logger.hpp"

VideoSource::VideoSource() 
{
    auto logger = Logger::get();
    cap_.open(0);
    if (!cap_.isOpened()) 
    {
        logger->error("Could not open video source");
        throw std::runtime_error("Could not open video source");
    }
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);

    logger->info("Video source initialized");
}

VideoSource::~VideoSource() 
{
    auto logger = Logger::get();
    logger->debug("VideoSource destructor called");
    close();
}

void VideoSource::close()
{
    if (cap_.isOpened()) 
    {
        auto logger = Logger::get();
        logger->info("Releasing video source");
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

    if (frame.empty()) 
    {
        auto logger = Logger::get();
        logger->warn("Captured empty frame from video source");
    }
    return frame;
}

void VideoSource::set_resolution(int width, int height) 
{
    auto logger = Logger::get();
    logger->debug("Setting resolution to {}x{}", width, height);
    
    width_ = width;
    height_ = height;
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);

    logger->info("Resolution set to {}x{}", width, height);
}