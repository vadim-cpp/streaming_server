#include "subtitle_receiver.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>

SubtitleReceiver::SubtitleReceiver(net::io_context& ioc, SubtitleCallback callback)
    : ioc_(ioc)
    , resolver_(net::make_strand(ioc))
    , ws_(net::make_strand(ioc))
    , callback_(std::move(callback))
{
}

void SubtitleReceiver::connect(const std::string& host, const std::string& port)
{
    host_ = host;
    port_ = port;
    
    auto logger = Logger::get();
    logger->info("Connecting to subtitle server: {}:{}", host, port);
    
    resolver_.async_resolve(host, port,
        beast::bind_front_handler(&SubtitleReceiver::on_resolve, shared_from_this()));
}

void SubtitleReceiver::close()
{
    if (connected_)
    {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
        connected_ = false;
    }
}

void SubtitleReceiver::on_resolve(beast::error_code ec, tcp::resolver::results_type results)
{
    auto logger = Logger::get();
    
    if (ec)
    {
        logger->error("Subtitle resolve failed: {}", ec.message());
        return;
    }
    
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
    
    beast::get_lowest_layer(ws_).async_connect(results,
        beast::bind_front_handler(&SubtitleReceiver::on_connect, shared_from_this()));
}

void SubtitleReceiver::on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep)
{
    auto logger = Logger::get();
    
    if (ec)
    {
        logger->error("Subtitle connect failed: {}", ec.message());
        return;
    }
    
    beast::get_lowest_layer(ws_).expires_never();
    
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
        req.set(http::field::user_agent, "ASCII-Streamer-Subtitle-Client");
    }));
    
    std::string host = host_ + ":" + std::to_string(ep.port());
    
    ws_.async_handshake(host, "/",
        beast::bind_front_handler(&SubtitleReceiver::on_handshake, shared_from_this()));
}

void SubtitleReceiver::on_handshake(beast::error_code ec)
{
    auto logger = Logger::get();
    
    if (ec)
    {
        logger->error("Subtitle handshake failed: {}", ec.message());
        return;
    }
    
    connected_ = true;
    logger->info("Connected to subtitle server successfully");
    
    // Начинаем чтение сообщений
    ws_.async_read(buffer_,
        beast::bind_front_handler(&SubtitleReceiver::on_read, shared_from_this()));
}

void SubtitleReceiver::on_read(beast::error_code ec, std::size_t bytes_transferred)
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
        // Парсим полученное сообщение
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        auto j = nlohmann::json::parse(message);
        std::string type = j.value("type", "");
        std::string text = j.value("text", "");
        
        if (type == "subtitle" && !text.empty())
        {
            logger->debug("Received subtitle: {}", text);
            if (callback_)
            {
                callback_(text);
            }
        }
    }
    catch (const std::exception& e)
    {
        logger->error("Failed to parse subtitle message: {}", e.what());
    }
    
    // Читаем следующее сообщение
    if (connected_)
    {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&SubtitleReceiver::on_read, shared_from_this()));
    }
}