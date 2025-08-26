#include "ascii_converter.hpp"
#include "logger.hpp"

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>

class AsciiConverterTest : public ::testing::Test 
{
protected:
    AsciiConverter converter;
    cv::Mat test_image;
    
    void SetUp() override 
    {
        converter.set_ascii_chars("@%#*+=-:. ");
        // Создаем цветное изображение (3 канала)
        test_image = cv::Mat::zeros(2, 2, CV_8UC3);
        test_image.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);      // черный
        test_image.at<cv::Vec3b>(0, 1) = cv::Vec3b(127, 127, 127); // серый
        test_image.at<cv::Vec3b>(1, 0) = cv::Vec3b(191, 191, 191); // светло-серый
        test_image.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 255, 255); // белый
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
    std::string expected = "@+\n: \n";
    EXPECT_EQ(result, expected);
}

TEST_F(AsciiConverterTest, ResizesCorrectly) 
{
    std::string result = converter.convert(test_image, 1, 1);
    // При уменьшении 2x2 до 1x1 используется интерполяция
    // Среднее значение пикселей: (0+127+191+255)/4 ≈ 143
    // Для набора символов "@%#*+=-:. " (10 символов)
    // 143 соответствует символу '='
    EXPECT_EQ(result, "=\n");
}

TEST_F(AsciiConverterTest, HandlesDifferentCharacterSets) 
{
    AsciiConverter custom;
    custom.set_ascii_chars("01");
    std::string result = custom.convert(test_image, 2, 2);
    // Для 2 символов пороги: 0-127 -> '0', 128-255 -> '1'
    std::string expected = "00\n11\n";
    EXPECT_EQ(result, expected);
}

TEST_F(AsciiConverterTest, HandlesNoAsciiCharsSet) 
{
    AsciiConverter no_chars_converter;
    no_chars_converter.set_ascii_chars("");
    std::string result = no_chars_converter.convert(test_image, 2, 2);
    EXPECT_EQ(result, "CONFIG ERROR");
}
