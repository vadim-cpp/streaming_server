#pragma once

#include "common_types.hpp"
#include "logger.hpp"
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;


class SubtitleReceiver : public std::enable_shared_from_this<SubtitleReceiver> 
{
public:
    using SubtitleCallback = std::function<void(const std::string&)>;
    using MicrophoneListCallback = std::function<void(const std::vector<MicrophoneInfo>&)>;

    SubtitleReceiver(net::io_context& ioc, SubtitleCallback subtitle_callback);
    ~SubtitleReceiver();

    void connect(const std::string& host, const std::string& port);
    void close();
    
    // Новые методы для работы с микрофонами
    void request_microphones_list();
    void start_audio_capture(int device_index);
    void stop_audio_capture();
    
    // Callback для получения списка микрофонов
    void set_microphone_list_callback(MicrophoneListCallback callback);
    
    bool is_connected() const { return connected_; }
    bool is_capturing() const { return audio_capturing_; }

private:
    net::io_context& ioc_;
    websocket::stream<beast::tcp_stream> ws_;
    tcp::resolver resolver_;
    beast::flat_buffer buffer_;

    SubtitleCallback subtitle_callback_;
    MicrophoneListCallback microphone_list_callback_;
    
    std::string host_;
    std::string port_;
    bool connected_ = false;
    bool audio_capturing_ = false;

    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);
    
    void handle_message(const std::string& message);
    void send_message(const nlohmann::json& message);
};