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
        // Устанавливаем стандартный набор ASCII-символов
        converter.set_ascii_chars("@%#*+=-:. ");
        
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
    
    // После преобразования в grayscale значения будут:
    // (0,0): 0 → '@'
    // (0,1): 127 → '+' (127/255 ≈ 0.5 → индекс 5 из 10 символов)
    // (1,0): 191 → '.' (191/255 ≈ 0.75 → индекс 7 из 10 символов)
    // (1,1): 255 → ' ' (255/255 = 1.0 → индекс 9 из 10 символов)
    std::string expected = "@+\n. \n";
    EXPECT_EQ(result, expected);
}

TEST_F(AsciiConverterTest, ResizesCorrectly) 
{
    std::string result = converter.convert(test_image, 1, 1);
    
    // При уменьшении до 1x1 будет усредненное значение яркости
    // (0+127+191+255)/4 = 143.25 → 143
    // 143/255 ≈ 0.56 → индекс 5 из 10 символов → '='
    EXPECT_EQ(result, "=\n");
}

TEST_F(AsciiConverterTest, HandlesDifferentCharacterSets) 
{
    AsciiConverter custom;
    custom.set_ascii_chars("01");
    
    std::string result = custom.convert(test_image, 2, 2);
    
    // Для набора "01" (2 символа):
    // (0,0): 0 → '0' (0/255 = 0 → индекс 0)
    // (0,1): 127 → '0' (127/255 ≈ 0.5 → индекс 0)
    // (1,0): 191 → '1' (191/255 ≈ 0.75 → индекс 1)
    // (1,1): 255 → '1' (255/255 = 1 → индекс 1)
    std::string expected = "00\n11\n";
    EXPECT_EQ(result, expected);
}

// Добавляем тест для проверки обработки ошибок
TEST_F(AsciiConverterTest, HandlesNoAsciiCharsSet) 
{
    AsciiConverter no_chars_converter;
    // Не устанавливаем ascii_chars_
    
    std::string result = no_chars_converter.convert(test_image, 2, 2);
    EXPECT_EQ(result, "CONFIG ERROR");
}