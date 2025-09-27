#include "subtitle_client.hpp"
#include "logger.hpp"

#include <nlohmann/json.hpp>

SubtitleClient::SubtitleClient(net::io_context& ioc) 
    : ioc_(ioc), resolver_(net::make_strand(ioc)), ws_(net::make_strand(ioc)) 
{
}

SubtitleClient::~SubtitleClient() 
{
    disconnect();
}

void SubtitleClient::connect(const std::string& host, const std::string& port, SubtitleCallback callback) 
{
    auto logger = Logger::get();
    
    host_ = host;
    port_ = port;
    subtitle_callback_ = std::move(callback);
    
    logger->info("Connecting to subtitle server at {}:{}", host, port);
    
    resolver_.async_resolve(host, port,
        beast::bind_front_handler(&SubtitleClient::on_resolve, shared_from_this()));
}

void SubtitleClient::send_audio_data(const std::vector<int16_t>& audio_data) 
{
    if (!connected_) return;
    
    // Отправляем аудиоданные как бинарное сообщение
    ws_.async_write(net::buffer(audio_data.data(), audio_data.size() * sizeof(int16_t)),
        beast::bind_front_handler(&SubtitleClient::on_write, shared_from_this()));
}

void SubtitleClient::disconnect() 
{
    if (connected_) 
    {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
        connected_ = false;
    }
}

void SubtitleClient::on_resolve(beast::error_code ec, tcp::resolver::results_type results) 
{
    auto logger = Logger::get();
    
    if (ec) 
    {
        logger->error("Subtitle client resolve failed: {}", ec.message());
        return;
    }
    
    beast::get_lowest_layer(ws_).async_connect(results,
        beast::bind_front_handler(&SubtitleClient::on_connect, shared_from_this()));
}

void SubtitleClient::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep) 
{
    auto logger = Logger::get();
    
    if (ec) 
    {
        logger->error("Subtitle client connect failed: {}", ec.message());
        return;
    }
    
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(beast::http::field::user_agent, "ASCII-Streamer-Subtitle-Client");
    }));
    
    std::string host = host_ + ":" + std::to_string(ep.port());
    ws_.async_handshake(host, "/",
        beast::bind_front_handler(&SubtitleClient::on_handshake, shared_from_this()));
}

void SubtitleClient::on_handshake(beast::error_code ec) 
{
    auto logger = Logger::get();
    
    if (ec) 
    {
        logger->error("Subtitle client handshake failed: {}", ec.message());
        return;
    }
    
    connected_ = true;
    logger->info("Connected to subtitle server successfully");
    
    // Начинаем чтение сообщений
    ws_.async_read(buffer_,
        beast::bind_front_handler(&SubtitleClient::on_read, shared_from_this()));
}

void SubtitleClient::on_read(beast::error_code ec, std::size_t bytes_transferred) 
{
    auto logger = Logger::get();
    
    if (ec == websocket::error::closed) 
    {
        logger->info("Subtitle connection closed");
        connected_ = false;
        return;
    }
    
    if (ec) 
    {
        logger->error("Subtitle read error: {}", ec.message());
        connected_ = false;
        return;
    }
    
    try 
    {
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        auto j = nlohmann::json::parse(message);
        std::string type = j.value("type", "");
        std::string text = j.value("text", "");
        
        if (type == "subtitle" && !text.empty() && subtitle_callback_) 
        {
            logger->debug("Received subtitle: {}", text);
            subtitle_callback_(text);
        }
    } 
    catch (const std::exception& e) 
    {
        logger->error("Failed to parse subtitle message: {}", e.what());
    }
    
    if (connected_) 
    {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&SubtitleClient::on_read, shared_from_this()));
    }
}

void SubtitleClient::on_write(beast::error_code ec, std::size_t bytes_transferred) 
{
    if (ec) 
    {
        auto logger = Logger::get();
        logger->error("Subtitle write error: {}", ec.message());
    }
}