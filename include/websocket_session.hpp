#pragma once

#include "server.hpp"
#include <boost/beast/websocket.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
namespace websocket = beast::websocket;

class websocket_session : public std::enable_shared_from_this<websocket_session> 
{
public:
    explicit websocket_session(beast::tcp_stream stream);
    void run(http::request<http::string_body> req);

private:
    void on_accept(beast::error_code ec);
    void do_read();
    void on_read(beast::error_code ec, size_t bytes);
    void on_close(beast::error_code ec);

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;
};