#include "video_source.hpp"
#include "logger.hpp"

#include <opencv2/videoio/registry.hpp>

std::vector<VideoSource::CameraInfo> VideoSource::list_cameras() 
{
    auto logger = Logger::get();
    std::vector<CameraInfo> cameras;
    
    // Проверяем первые 10 индексов
    for (int i = 0; i < 10; ++i) 
    {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) 
        {
            CameraInfo info;
            info.index = i;
            
            // Пытаемся получить имя камеры
            try 
            {
                info.name = "Camera " + std::to_string(i);
                #ifdef _WIN32
                // На Windows можно попробовать получить имя через свойства
                char buffer[256];
                if (cap.get(cv::CAP_PROP_BACKEND) == cv::CAP_DSHOW) 
                {
                    sprintf(buffer, "Camera %d", i);
                    info.name = buffer;
                }
                #endif
            } 
            catch (...) 
            {
                info.name = "Camera " + std::to_string(i);
            }
            
            cameras.push_back(info);
            cap.release();
        }
    }
    
    logger->info("Found {} available cameras", cameras.size());
    return cameras;
}

VideoSource::VideoSource() 
{
    auto logger = Logger::get();
    logger->debug("VideoSource created");
}

VideoSource::~VideoSource() 
{
    auto logger = Logger::get();
    logger->debug("VideoSource destructor called");
    close();
}

void VideoSource::open(int index) 
{
    auto logger = Logger::get();
    
    if (cap_.isOpened()) 
    {
        close();
    }
    
    camera_index_ = index;
    cap_.open(camera_index_);
    
    if (!cap_.isOpened()) 
    {
        logger->error("Could not open video source at index {}", camera_index_);
        throw std::runtime_error("Could not open video source");
    }
    
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    is_opened_ = true;

    logger->info("Video source initialized at index {}", camera_index_);
}

void VideoSource::close()
{
    if (cap_.isOpened()) 
    {
        auto logger = Logger::get();
        logger->info("Releasing video source");
        cap_.release();
        is_opened_ = false;
    }
}

bool VideoSource::is_available() const 
{
    return is_opened_;
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