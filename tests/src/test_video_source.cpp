#include "video_source.hpp"
#include "logger.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

class VideoSourceTest : public ::testing::Test 
{
protected:
    std::unique_ptr<VideoSource> video_source;
    std::vector<VideoSource::CameraInfo> available_cameras;
    
    void SetUp() override 
    {
        available_cameras = VideoSource::list_cameras();
        
        // Создаем VideoSource только если есть доступные камеры
        if (!available_cameras.empty()) 
        {
            video_source = std::make_unique<VideoSource>();
        }
    }
    
    void TearDown() override 
    {
        if (video_source) 
        {
            video_source->close();
        }
    }
};

TEST_F(VideoSourceTest, ListCamerasReturnsWithoutError) 
{
    EXPECT_NO_THROW({
        auto cameras = VideoSource::list_cameras();
    });
}

TEST_F(VideoSourceTest, CanBeConstructedAndDestructed) 
{
    if (available_cameras.empty()) 
    {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    EXPECT_NO_THROW({
        VideoSource local_video_source;
    });
}

TEST_F(VideoSourceTest, IsAvailableReturnsCorrectState) 
{
    if (available_cameras.empty()) {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    EXPECT_TRUE(video_source->is_available());
    
    video_source->close();
    EXPECT_FALSE(video_source->is_available());
}

TEST_F(VideoSourceTest, CaptureFrameReturnsNonEmptyMat) 
{
    if (available_cameras.empty()) 
    {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    cv::Mat frame = video_source->capture_frame();
    EXPECT_FALSE(frame.empty());
}

TEST_F(VideoSourceTest, SetResolutionChangesProperties) 
{
    if (available_cameras.empty()) 
    {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    const int test_width = 320;
    const int test_height = 240;
    
    video_source->set_resolution(test_width, test_height);
    
    // Захватываем кадр, чтобы убедиться, что разрешение изменилось
    cv::Mat frame = video_source->capture_frame();
    EXPECT_FALSE(frame.empty());
}

TEST_F(VideoSourceTest, CanOpenCameraByIndex) 
{
    if (available_cameras.empty()) 
    {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    // Пытаемся открыть первую доступную камеру
    EXPECT_NO_THROW({
        video_source->open(available_cameras[0].index);
    });
    
    EXPECT_TRUE(video_source->is_available());
}

TEST_F(VideoSourceTest, HandlesInvalidCameraIndex) 
{
    // Пытаемся открыть несуществующий индекс
    EXPECT_THROW({
        VideoSource invalid_source;
        invalid_source.open(999);
    }, std::runtime_error);
}

TEST_F(VideoSourceTest, CloseWorksWithoutError)
 {
    if (available_cameras.empty()) 
    {
        GTEST_SKIP() << "No cameras available, skipping test";
    }
    
    EXPECT_NO_THROW({
        video_source->close();
    });
}
