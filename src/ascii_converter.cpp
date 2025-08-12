#include "ascii_converter.hpp"
#include "logger.hpp"

#include <opencv2/opencv.hpp>

AsciiConverter::AsciiConverter() {}

cv::Mat AsciiConverter::resize_frame(const cv::Mat& frame, int width, int height) 
{
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(width, height));
    return resized;
}

cv::Mat AsciiConverter::convert_to_grayscale(const cv::Mat& frame) 
{
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

void AsciiConverter::set_ascii_chars(const std::string& chars) 
{
    ascii_chars_ = chars;
}

std::string AsciiConverter::convert(const cv::Mat& frame, int output_width, int output_height) 
{
    auto logger = Logger::get();

    if (frame.empty()) 
    {
        logger->warn("Attempted to convert empty frame");
        return "";
    }

    if (ascii_chars_.empty()) 
    {
        logger->error("ASCII characters not set");
        return "CONFIG ERROR";
    }

    logger->debug("Converting frame to ASCII: {}x{}", output_width, output_height);
    
    // Обработка кадра
    cv::Mat processed = convert_to_grayscale(frame);
    processed = resize_frame(processed, output_width, output_height);
    
    // Создаем lookup table (LUT)
    const size_t num_chars = ascii_chars_.size();
    const double char_step = 255.0 / (num_chars - 1);
    std::array<char, 256> lut;
    for (int i = 0; i < 256; ++i) 
    {
        int index = static_cast<int>(i / char_step + 0.5);  // Округление
        lut[i] = ascii_chars_[std::clamp(index, 0, static_cast<int>(num_chars - 1))];
    }

    // Рассчитываем необходимый размер буфера
    const size_t rows = processed.rows;
    const size_t cols = processed.cols;
    const size_t buffer_size = rows * (cols + 1);  // +1 для '\n' в каждой строке
    
    // Резервируем память (+1 для null-terminator)
    std::string ascii_frame;
    ascii_frame.reserve(buffer_size + 1);
    
    // Быстрая конвертация с использованием указателей
    for (int y = 0; y < rows; ++y) 
    {
        const uchar* row_ptr = processed.ptr<uchar>(y);
        
        for (int x = 0; x < cols; ++x) 
        {
            ascii_frame += lut[row_ptr[x]];
        }
        
        ascii_frame += '\n';
    }
    
    return ascii_frame;
}