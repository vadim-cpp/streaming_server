#include "stream_controller.hpp"
#include "video_source_interface.hpp"
#include "ascii_converter_interface.hpp"
#include "logger.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <opencv2/opencv.hpp>


class MockVideoSource : public IVideoSource 
{
public:
    MOCK_METHOD(bool, is_available, (), (const, override));
    MOCK_METHOD(cv::Mat, capture_frame, (), (override));
    MOCK_METHOD(void, set_resolution, (int width, int height), (override));
    MOCK_METHOD(void, open, (int index), (override));
    MOCK_METHOD(void, close, (), (override));
};

class MockAsciiConverter : public IAsciiConverter 
{
public:
    MOCK_METHOD(void, set_ascii_chars, (const std::string& chars), (override));
    MOCK_METHOD(std::string, convert, (const cv::Mat& frame, int width, int height), (override));
};

MATCHER_P(MatEquals, expected, "") 
{
    if (arg.empty() && expected.empty()) return true;
    if (arg.empty() || expected.empty()) return false;
    if (arg.size() != expected.size()) return false;
    
    cv::Mat arg_gray, expected_gray;
    if (arg.channels() > 1) 
    {
        cv::cvtColor(arg, arg_gray, cv::COLOR_BGR2GRAY);
    } 
    else 
    {
        arg_gray = arg;
    }
    
    if (expected.channels() > 1) 
    {
        cv::cvtColor(expected, expected_gray, cv::COLOR_BGR2GRAY);
    } 
    else 
    {
        expected_gray = expected;
    }
    
    cv::Mat diff;
    cv::compare(arg_gray, expected_gray, diff, cv::CMP_NE);
    return cv::countNonZero(diff) == 0;
}

class StreamControllerTest : public testing::Test 
{
protected:
    void SetUp() override 
    {
        static bool logger_initialized = false;
        if (!logger_initialized) 
        {
            Logger::init();
            logger_initialized = true;
        }
        
        video_source_ = std::make_shared<testing::NiceMock<MockVideoSource>>();
        ascii_converter_ = std::make_shared<testing::NiceMock<MockAsciiConverter>>();
        controller_ = std::make_shared<StreamController>(ioc_, video_source_, ascii_converter_);
    }

    void TearDown() override 
    {
        if (controller_->is_streaming()) 
        {
            boost::asio::co_spawn(ioc_, 
                [&]() -> net::awaitable<void> {
                    co_await controller_->stop_streaming();
                }, 
                boost::asio::detached);
            
            ioc_.run_for(std::chrono::milliseconds(100));
        }
        
        controller_.reset();
        video_source_.reset();
        ascii_converter_.reset();
        
        ioc_.restart();
    }

    net::io_context ioc_;
    std::shared_ptr<testing::NiceMock<MockVideoSource>> video_source_;
    std::shared_ptr<testing::NiceMock<MockAsciiConverter>> ascii_converter_;
    std::shared_ptr<StreamController> controller_;
};

TEST_F(StreamControllerTest, StartStreamingSuccess) 
{
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(cv::Mat()));

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller_->is_streaming());
}

TEST_F(StreamControllerTest, StartStreamingAlreadyActive) 
{
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(cv::Mat()));

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
            co_await controller_->start_streaming(0, "120x90", 10);
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(controller_->is_streaming());
}

TEST_F(StreamControllerTest, StopStreamingSuccess) 
{
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(cv::Mat()));

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
            co_await controller_->stop_streaming();
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(200));
    
    EXPECT_FALSE(controller_->is_streaming());
}

TEST_F(StreamControllerTest, StopStreamingNotActive) 
{
    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->stop_streaming();
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(controller_->is_streaming());
}

TEST_F(StreamControllerTest, CaptureLoopEmptyFrame) 
{
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    cv::Mat empty_frame;
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(empty_frame));

    EXPECT_CALL(*ascii_converter_, convert(testing::_, testing::_, testing::_)).Times(0);

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
}

TEST_F(StreamControllerTest, CaptureLoopValidFrame) 
{
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    cv::Mat test_frame = cv::Mat::ones(180, 240, CV_8UC1) * 128;
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(test_frame));

    EXPECT_CALL(*ascii_converter_, convert(testing::_, 120, 90))
        .WillOnce(testing::Return("test_ascii_frame"));

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
}

TEST_F(StreamControllerTest, GetStatus) 
{
    EXPECT_EQ(controller_->get_status(), "inactive");
    
    EXPECT_CALL(*video_source_, open(0)).Times(1);
    EXPECT_CALL(*video_source_, set_resolution(240, 180)).Times(1);
    EXPECT_CALL(*ascii_converter_, set_ascii_chars("@%#*+=-:. ")).Times(1);
    
    EXPECT_CALL(*video_source_, capture_frame())
        .WillOnce(testing::Return(cv::Mat()));

    boost::asio::co_spawn(ioc_, 
        [&]() -> net::awaitable<void> {
            co_await controller_->start_streaming(0, "120x90", 10);
            EXPECT_EQ(controller_->get_status(), "active");
            co_await controller_->stop_streaming();
            EXPECT_EQ(controller_->get_status(), "inactive");
        }, 
        boost::asio::detached);
    
    ioc_.run_for(std::chrono::milliseconds(100));
}
