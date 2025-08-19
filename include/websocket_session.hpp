#pragma once

#include "server.hpp"
#include "video_source_interface.hpp"
#include "ascii_converter_interface.hpp"
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;
namespace websocket = beast::websocket;

class websocket_session : public std::enable_shared_from_this<websocket_session> 
{
public:
    explicit websocket_session(
        beast::tcp_stream stream,
        std::unique_ptr<IVideoSource> video_source,
        std::unique_ptr<IAsciiConverter> ascii_converter);
    
    ~websocket_session();
    void run(http::request<http::string_body> req);

private:
    net::awaitable<void> do_run(http::request<http::string_body> req);
    net::awaitable<void> do_read();
    net::awaitable<void> handle_message(const std::string& message);
    net::awaitable<void> handle_config(const nlohmann::json& j);
    net::awaitable<void> start_streaming();
    net::awaitable<void> send_frames();

    void release_camera();

    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    std::unique_ptr<IVideoSource> video_source_;
    std::unique_ptr<IAsciiConverter> ascii_converter_;
    net::steady_timer frame_timer_;
    int frame_width_ = 120;
    int frame_height_ = 90;
    int fps_ = 10;
    bool is_streaming_ = false;
};