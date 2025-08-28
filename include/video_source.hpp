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
    void open(int index) override;
    void close() override;

    struct CameraInfo {
        int index;
        std::string name;
    };
    static std::vector<CameraInfo> list_cameras();
    
private:
    cv::VideoCapture cap_;
    int camera_index_{-1};
    int width_{640};
    int height_{480};
    bool is_opened_{false};
};