#include "ascii_converter.hpp"
#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

class AsciiConverterTest : public ::testing::Test 
{
protected:
    AsciiConverter converter;
    cv::Mat test_image;
    
    void SetUp() override 
    {
        // Создаем тестовое изображение 2x2 пикселя
        test_image = cv::Mat::zeros(2, 2, CV_8UC3);
        
        // Черный пиксель
        test_image.at<cv::Vec3b>(0,0) = cv::Vec3b(0,0,0);
        // Серый пиксель (50%)
        test_image.at<cv::Vec3b>(0,1) = cv::Vec3b(127,127,127);
        // Серый пиксель (75%)
        test_image.at<cv::Vec3b>(1,0) = cv::Vec3b(191,191,191);
        // Белый пиксель
        test_image.at<cv::Vec3b>(1,1) = cv::Vec3b(255,255,255);
    }
};

TEST_F(AsciiConverterTest, HandlesEmptyFrame) 
{
    cv::Mat empty_frame;
    std::string result = converter.convert(empty_frame, 100, 100);
    EXPECT_TRUE(result.empty());
}

TEST_F(AsciiConverterTest, ConvertsGrayscaleCorrectly) 
{
    std::string result = converter.convert(test_image, 2, 2);
    
    std::string expected = "@+\n. \n";
    EXPECT_EQ(result, expected);
}

TEST_F(AsciiConverterTest, ResizesCorrectly) 
{
    std::string result = converter.convert(test_image, 1, 1);
    
    EXPECT_EQ(result, "=\n");
}

TEST_F(AsciiConverterTest, HandlesDifferentCharacterSets) 
{
    AsciiConverter custom;
    custom.set_ascii_chars("01");
    
    std::string result = custom.convert(test_image, 2, 2);
    std::string expected = "01\n11\n";
    EXPECT_EQ(result, expected);
}