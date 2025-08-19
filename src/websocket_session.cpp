#include "websocket_session.hpp"
#include "video_source.hpp"
#include "ascii_converter.hpp"
#include "logger.hpp"
#include "server.hpp"

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
    auto logger = Logger::get();
    logger->debug("WebSocket session destroyed");
    frame_timer_.cancel();
}

void websocket_session::run(http::request<http::string_body> req) 
{
    net::co_spawn(
        ws_.get_executor(),
        [self = shared_from_this(), req = std::move(req)]() mutable {
            return self->do_run(std::move(req));
        },
        net::detached
    );
}

net::awaitable<void> websocket_session::do_run(http::request<http::string_body> req) 
{
    auto logger = Logger::get();
    
    try 
    {
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));
        
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server, "ASCII Stream Server");
            }));
        
        co_await ws_.async_accept(req, net::use_awaitable);
        logger->info("WebSocket connection established");
        
        while (ws_.is_open()) 
        {
            co_await do_read();
        }
    } 
    catch (const std::exception& e) 
    {
        logger->error("WebSocket error: {}", e.what());
    }
    
    logger->info("WebSocket connection closed");
    release_camera();
}

net::awaitable<void> websocket_session::do_read() 
{
    auto logger = Logger::get();
    
    try 
    {
        buffer_.clear();
        auto bytes = co_await ws_.async_read(buffer_, net::use_awaitable);
        
        auto message = beast::buffers_to_string(buffer_.data());
        logger->debug("Received message: {}", message);
        
        co_await handle_message(message);
    } 
    catch (const beast::system_error& e) 
    {
        if (e.code() != websocket::error::closed) 
        {
            logger->error("WebSocket read error: {}", e.what());
        }
        throw;
    }
}

net::awaitable<void> websocket_session::handle_message(const std::string& message) 
{
    auto logger = Logger::get();
    
    try 
    {
        auto j = nlohmann::json::parse(message);
        std::string type = j["type"];
        
        if (type == "config") 
        {
            co_await handle_config(j);
        } 
        else 
        if (type == "stop") 
        {
            is_streaming_ = false;
            frame_timer_.cancel();
            release_camera();
            co_await ws_.async_write(net::buffer("Stream stopped"), net::use_awaitable);
        } 
        else if (type == "auth") 
        {
            std::string received_key = j["api_key"];
            co_await ws_.async_write(net::buffer("AUTH_SUCCESS"), net::use_awaitable);
            
            if (!is_streaming_) 
            {
                co_await start_streaming();
            }
        } 
        else 
        {
            co_await ws_.async_write(buffer_.data(), net::use_awaitable);
        }
    } 
    catch (const std::exception& e) 
    {
        logger->error("Message handling error: {}", e.what());
        std::string error_msg = "ERROR: " + std::string(e.what());
        //co_await ws_.async_write(net::buffer(error_msg), net::use_awaitable);
    }
}

net::awaitable<void> websocket_session::handle_config(const nlohmann::json& j) 
{
    auto logger = Logger::get();
    
    try 
    {
        // Parse resolution "120x90"
        std::string res = j["resolution"];
        size_t pos = res.find('x');
        frame_width_ = std::stoi(res.substr(0, pos));
        frame_height_ = std::stoi(res.substr(pos + 1));

        int camera_index = j.value("camera_index", 0);
        
        if (j.contains("fps")) 
        {
            fps_ = j["fps"];
        }
        
        release_camera();
        video_source_ = std::make_unique<VideoSource>();
        video_source_->open(camera_index);
        video_source_->set_resolution(frame_width_ * 2, frame_height_ * 2);
        
        logger->info("New stream config: {} FPS, resolution: {}x{}", 
                    fps_, frame_width_, frame_height_);
        
        co_await start_streaming();
    } 
    catch (const std::exception& e) 
    {
        logger->error("Config error: {}", e.what());
        throw;
    }
}

net::awaitable<void> websocket_session::start_streaming() 
{
    auto logger = Logger::get();
    
    if (is_streaming_) 
    {
        co_return;
    }
    
    if (!video_source_ || !video_source_->is_available()) 
    {
        throw std::runtime_error("Video source not available");
    }
    
    logger->info("Starting video streaming");
    is_streaming_ = true;
    
    net::co_spawn(
        ws_.get_executor(),
        [self = shared_from_this()] {
            return self->send_frames();
        },
        net::detached
    );
}

net::awaitable<void> websocket_session::send_frames() 
{
    auto logger = Logger::get();
    
    while (is_streaming_ && ws_.is_open()) 
    {
        try 
        {
            cv::Mat frame = video_source_->capture_frame();
            if (frame.empty()) 
            {
                logger->warn("Empty frame captured, skipping");
                co_await net::steady_timer(ws_.get_executor(), 
                    std::chrono::milliseconds(1000 / fps_)).async_wait(net::use_awaitable);
                continue;
            }
            
            std::string ascii_data = ascii_converter_->convert(frame, frame_width_, frame_height_);
            co_await ws_.async_write(net::buffer(ascii_data), net::use_awaitable);
            
            co_await net::steady_timer(ws_.get_executor(), 
                std::chrono::milliseconds(1000 / fps_)).async_wait(net::use_awaitable);
        } 
        catch (const std::exception& e) 
        {
            logger->error("Frame sending error: {}", e.what());
            break;
        }
    }
    
    is_streaming_ = false;
}

void websocket_session::release_camera()
{
    if (video_source_) 
    {
        auto logger = Logger::get();
        logger->info("Releasing camera resources");
        video_source_->close();
        video_source_.reset();
    }
}