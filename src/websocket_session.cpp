#include "websocket_session.hpp"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

websocket_session::websocket_session(beast::tcp_stream stream)
    : ws_(std::move(stream)),
      frame_timer_(ws_.get_executor()) 
{
    video_source_ = std::make_unique<VideoSource>();
    ascii_converter_ = std::make_unique<AsciiConverter>();
}

void websocket_session::run(http::request<http::string_body> req) 
{
    // Устанавливаем таймауты
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server));
    
    // Устанавливаем обработчик для изменения ответа
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "ASCII Stream Server");
        }));
    
    // Принимаем WebSocket соединение
    ws_.async_accept(
        req,
        [self = shared_from_this()](beast::error_code ec) {
            self->on_accept(ec);
        });
}

void websocket_session::on_accept(beast::error_code ec) 
{
    if(ec) 
    {
        std::cerr << "Accept error: " << ec.message() << "\n";
        return;
    }
    do_read();
}

void websocket_session::do_read() 
{
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void websocket_session::on_read(beast::error_code ec, size_t) 
{
    if(ec == websocket::error::closed) 
    {
        return on_close(ec);
    }
    
    if(ec) 
    {
        std::cerr << "Read error: " << ec.message() << "\n";
        return;
    }

        auto message = beast::buffers_to_string(buffer_.data());
    
    // Проверяем конфигурационное сообщение
    if (message.find("\"type\":\"config\"") != std::string::npos) {
        handle_config(message);
        buffer_.consume(buffer_.size());
        return;
    }
    
    // Эхо-ответ
    ws_.text(ws_.got_text());
    ws_.async_write(
        buffer_.data(),
        [self = shared_from_this()](beast::error_code ec, size_t) {
            if(ec) 
            {
                std::cerr << "Write error: " << ec.message() << "\n";
                return;
            }
            
            self->buffer_.consume(self->buffer_.size());
            self->do_read();
        });
}

void websocket_session::on_close(beast::error_code ec) 
{
    if(ec && ec != beast::errc::not_connected) 
    {
        std::cerr << "Close error: " << ec.message() << "\n";
    }
}

void websocket_session::handle_config(const std::string& json) 
{
    try 
    {
        // Парсим JSON: {"type":"config","resolution":"120x90","fps":10}
        auto j = nlohmann::json::parse(json);
        
        if (j["type"] == "config") {
            // Парсим разрешение "120x90"
            std::string res = j["resolution"];
            size_t pos = res.find('x');
            frame_width_ = std::stoi(res.substr(0, pos));
            frame_height_ = std::stoi(res.substr(pos + 1));
            
            // Устанавливаем FPS если есть
            if (j.contains("fps")) {
                fps_ = j["fps"];
            }
            
            // Настраиваем источник видео
            video_source_->set_resolution(frame_width_ * 2, frame_height_ * 2);
            
            // Запускаем стриминг
            start_streaming();
        }
    } 
    catch (...) 
    {
        // Обработка ошибок парсинга
    }
}

void websocket_session::start_streaming() 
{
    if (!video_source_->is_available()) {
        // Отправляем ошибку клиенту
        ws_.async_write(net::buffer("Error: Video source not available"));
        return;
    }
    
    is_streaming_ = true;
    send_next_frame();
}

void websocket_session::send_next_frame() 
{
    if (!is_streaming_) return;
    
    // Захватываем и конвертируем кадр
    cv::Mat frame = video_source_->capture_frame();
    std::string ascii_frame = ascii_converter_->convert(frame, frame_width_, frame_height_);
    
    // Отправляем клиенту
    ws_.text(true);
    ws_.async_write(
        net::buffer(ascii_frame),
        [self = shared_from_this()](beast::error_code ec, size_t) {
            if (ec) {
                self->is_streaming_ = false;
                return;
            }
            
            // Запускаем таймер для следующего кадра
            self->frame_timer_.expires_after(std::chrono::milliseconds(1000 / self->fps_));
            self->frame_timer_.async_wait(
                [self](beast::error_code ec) {
                    if (!ec) self->send_next_frame();
                }
            );
        }
    );
}

