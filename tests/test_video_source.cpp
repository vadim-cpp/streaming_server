#include "video_source.hpp"
#include "video_source_interface.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class MockVideoSource : public IVideoSource
{
public:
    MOCK_METHOD(bool, is_available, (), (const, override));
    MOCK_METHOD(cv::Mat, capture_frame, (), (override));
    MOCK_METHOD(void, set_resolution, (int width, int height), (override));
    MOCK_METHOD(void, close, (), (override));
};

TEST(VideoSourceTest, CapturesFrame) 
{
    NiceMock<MockVideoSource> mock;
    
    // Настраиваем mock для возврата тестового кадра
    cv::Mat test_frame = cv::Mat::zeros(480, 640, CV_8UC3);
    EXPECT_CALL(mock, capture_frame())
        .WillOnce(Return(test_frame));
    
    cv::Mat result = mock.capture_frame();
    EXPECT_FALSE(result.empty());
    EXPECT_EQ(result.rows, 480);
    EXPECT_EQ(result.cols, 640);
}

TEST(VideoSourceTest, SetsResolution) 
{
    MockVideoSource mock;
    
    EXPECT_CALL(mock, set_resolution(320, 240))
        .Times(1);
    
    mock.set_resolution(320, 240);
}

TEST(VideoSourceTest, ChecksAvailability) 
{
    MockVideoSource mock;
    
    EXPECT_CALL(mock, is_available())
        .WillOnce(Return(true))
        .WillOnce(Return(false));
    
    EXPECT_TRUE(mock.is_available());
    EXPECT_FALSE(mock.is_available());
}