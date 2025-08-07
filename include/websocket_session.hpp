#pragma once

#include "server.hpp"
#include "video_source.hpp"
#include "ascii_converter.hpp"

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

    void handle_config(const std::string& json);
    void start_streaming();
    void send_next_frame();

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    std::unique_ptr<VideoSource> video_source_;
    std::unique_ptr<AsciiConverter> ascii_converter_;
    net::steady_timer frame_timer_;
    int frame_width_ = 120;
    int frame_height_ = 90;
    int fps_ = 10;
    bool is_streaming_ = false;
};