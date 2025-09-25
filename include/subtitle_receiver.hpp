#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <string>
#include <memory>
#include <functional>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class SubtitleReceiver : public std::enable_shared_from_this<SubtitleReceiver>
{
public:
    using SubtitleCallback = std::function<void(const std::string&)>;
    
    SubtitleReceiver(net::io_context& ioc, SubtitleCallback callback);
    void connect(const std::string& host, const std::string& port);
    void close();
    
private:
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results);
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type ep);
    void on_handshake(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void run();
    
    net::io_context& ioc_;
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
    SubtitleCallback callback_;
    std::string host_;
    std::string port_;
    bool connected_ = false;
};
