#include "subtitle_receiver.hpp"

SubtitleReceiver::SubtitleReceiver(net::io_context& ioc, SubtitleCallback subtitle_callback)
    : ioc_(ioc)
    , resolver_(net::make_strand(ioc))
    , ws_(net::make_strand(ioc))
    , subtitle_callback_(std::move(subtitle_callback))
{
}

SubtitleReceiver::~SubtitleReceiver()
{
    close();
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
        audio_capturing_ = false;
    }
}

void SubtitleReceiver::request_microphones_list()
{
    if (!connected_) 
    {
        auto logger = Logger::get();
        logger->warn("Not connected to subtitle server");
        return;
    }
    
    nlohmann::json message;
    message["type"] = "list_microphones";
    
    send_message(message);
}

void SubtitleReceiver::start_audio_capture(int device_index)
{
    if (!connected_) 
    {
        auto logger = Logger::get();
        logger->warn("Not connected to subtitle server");
        return;
    }
    
    nlohmann::json message;
    message["type"] = "start_capture";
    message["device_index"] = device_index;
    
    send_message(message);
    audio_capturing_ = true;
}

void SubtitleReceiver::stop_audio_capture()
{
    if (!connected_) return;
    
    nlohmann::json message;
    message["type"] = "stop_capture";
    
    send_message(message);
    audio_capturing_ = false;
}

void SubtitleReceiver::set_microphone_list_callback(MicrophoneListCallback callback)
{
    microphone_list_callback_ = std::move(callback);
}

void SubtitleReceiver::send_message(const nlohmann::json& message)
{
    std::string message_str = message.dump();
    ws_.async_write(net::buffer(message_str),
        beast::bind_front_handler(&SubtitleReceiver::on_write, shared_from_this()));
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
        audio_capturing_ = false;
        return;
    }
    
    if (ec)
    {
        logger->error("Subtitle read error: {}", ec.message());
        connected_ = false;
        audio_capturing_ = false;
        return;
    }
    
    try
    {
        std::string message = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        handle_message(message);
    }
    catch (const std::exception& e)
    {
        logger->error("Failed to process subtitle message: {}", e.what());
    }
    
    // Читаем следующее сообщение
    if (connected_)
    {
        ws_.async_read(buffer_,
            beast::bind_front_handler(&SubtitleReceiver::on_read, shared_from_this()));
    }
}

void SubtitleReceiver::handle_message(const std::string& message)
{
    auto logger = Logger::get();
    
    try
    {
        auto j = nlohmann::json::parse(message);
        std::string type = j.value("type", "");
        
        if (type == "subtitle")
        {
            std::string text = j.value("text", "");
            if (!text.empty() && subtitle_callback_)
            {
                logger->debug("Received subtitle: {}", text);
                subtitle_callback_(text);
            }
        }
        else if (type == "microphones_list")
        {
            if (microphone_list_callback_ && j.contains("microphones"))
            {
                std::vector<MicrophoneInfo> microphones;
                const auto& mics_json = j["microphones"];
                
                if (mics_json.is_array()) {
                    for (const auto& mic_json : mics_json) {
                        MicrophoneInfo mic;
                        mic.index = mic_json.value("index", -1);
                        mic.name = mic_json.value("name", "");
                        mic.id = mic_json.value("id", "");
                        mic.sample_rate = mic_json.value("sample_rate", 0);
                        microphones.push_back(mic);
                    }
                }
                
                microphone_list_callback_(microphones);
                logger->info("Received {} microphones from subtitle server", microphones.size());
            }
        }
        else if (type == "capture_started")
        {
            audio_capturing_ = true;
            logger->info("Audio capture started on subtitle server");
        }
        else if (type == "capture_stopped")
        {
            audio_capturing_ = false;
            logger->info("Audio capture stopped on subtitle server");
        }
        else if (type == "error")
        {
            std::string error_msg = j.value("message", "Unknown error");
            logger->error("Subtitle server error: {}", error_msg);
        }
    }
    catch (const std::exception& e)
    {
        logger->error("Failed to parse subtitle message: {}", e.what());
    }
}

void SubtitleReceiver::on_write(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec)
    {
        auto logger = Logger::get();
        logger->error("Subtitle write error: {}", ec.message());
    }
}