#include "websocket_session.hpp"
#include "video_source.hpp"
#include "ascii_converter.hpp"

#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <nlohmann/json.hpp>


namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

websocket_session::websocket_session(
    beast::tcp_stream stream,
    std::unique_ptr<IVideoSource> video_source,
    std::unique_ptr<IAsciiConverter> ascii_converter)
    : ws_(std::move(stream)),
      video_source_(std::move(video_source)),
      ascii_converter_(std::move(ascii_converter)),
      frame_timer_(ws_.get_executor()) 
{}

websocket_session::~websocket_session() 
{
    if (video_source_) 
    {
        video_source_.reset();
    }
    frame_timer_.cancel();
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
    
    // Обработка конфигурации
    if (message.find("\"type\":\"config\"") != std::string::npos) 
    {
        handle_config(message);
        buffer_.consume(buffer_.size());
        return;
    }
    
    // Обработка команды остановки
    if (message.find("\"type\":\"stop\"") != std::string::npos) 
    {
        is_streaming_ = false;
        frame_timer_.cancel();
        release_camera();
        
        ws_.async_write(net::buffer("Stream stopped"),
            [self = shared_from_this()](beast::error_code ec, size_t) {
                if (!ec) self->do_read();
            });
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

void websocket_session::release_camera()
{
    if (video_source_) 
    {
        video_source_->close();
        video_source_.reset();
    }
}

void websocket_session::on_close(beast::error_code ec) 
{
    is_streaming_ = false;
    frame_timer_.cancel();
    release_camera();
    
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
            
            // Пересоздаем видео источник при новой конфигурации
            release_camera();
            video_source_ = std::make_unique<VideoSource>();
            video_source_->set_resolution(frame_width_ * 2, frame_height_ * 2);
            
            start_streaming();
        }
    } 
    catch (const nlohmann::json::exception& e) 
    {
        ws_.async_write(net::buffer("JSON error: " + std::string(e.what())));
    }
}

void websocket_session::start_streaming() 
{
    if (is_streaming_) return;
    
    if (!video_source_) 
    {
        try 
        {
            video_source_ = std::make_unique<VideoSource>();
            video_source_->set_resolution(frame_width_ * 2, frame_height_ * 2);
        } 
        catch (const std::exception& e) 
        {
            ws_.async_write(net::buffer("Error: " + std::string(e.what())),
                [](beast::error_code, size_t) {});
            return;
        }
    }
    
    if (!video_source_->is_available()) 
    {
        ws_.async_write(net::buffer("Error: Video source not available")),
            [](beast::error_code, size_t) {};
        return;
    }
    
    is_streaming_ = true;
    send_next_frame();
}

void websocket_session::send_next_frame() 
{
    if (!is_streaming_) return;
    
    cv::Mat frame = video_source_->capture_frame();
    if (frame.empty()) 
    {
        // Обработка ошибки
        return;
    }
    
    // Создаём shared_ptr для управления временем жизни
    auto ascii_data = std::make_shared<std::string>(
        ascii_converter_->convert(frame, frame_width_, frame_height_)
    );
    
    ws_.text(true);
    ws_.async_write(
        net::buffer(*ascii_data),
        [self = shared_from_this(), ascii_data](beast::error_code ec, size_t) {
            // ascii_data будет существовать до завершения лямбды
            if (ec) 
            {
                self->is_streaming_ = false;
                return;
            }
            
            self->frame_timer_.expires_after(std::chrono::milliseconds(1000 / self->fps_));
            self->frame_timer_.async_wait(
                [self](beast::error_code ec) {
                    if (!ec) self->send_next_frame();
                }
            );
        }
    );
}

