#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>
#include <functional>
#include <memory>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

class SubtitleClient : public std::enable_shared_from_this<SubtitleClient> 
{
public:
    using SubtitleCallback = std::function<void(const std::string& subtitle)>;

    SubtitleClient(net::io_context& ioc);
    ~SubtitleClient();

    void connect(const std::string& host, const std::string& port, SubtitleCallback callback);
    void send_audio_data(const std::vector<int16_t>& audio_data);
    void disconnect();
    bool is_connected() const { return connected_; }

private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

    net::io_context& ioc_;
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    
    std::string host_;
    std::string port_;
    SubtitleCallback subtitle_callback_;
    std::atomic<bool> connected_{false};
};