#pragma once

#include "stream_controller.hpp"

#include <memory>
#include <deque>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;

class Server;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> 
{
public:
    WebSocketSession(beast::tcp_stream stream, std::shared_ptr<StreamController> controller, std::shared_ptr<Server> server);
    ~WebSocketSession();
    
    void run(http::request<http::string_body> req);
    void send_frame(const std::string& frame);
    void close();

private:
    net::awaitable<void> do_run(http::request<http::string_body> req);
    net::awaitable<void> do_read();
    net::awaitable<void> handle_message(const std::string& message);
    net::awaitable<void> do_write();
    
    websocket::stream<beast::tcp_stream> ws_;
    std::shared_ptr<StreamController> controller_;
    std::shared_ptr<Server> server_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
    bool is_writing_ = false;
    bool is_authenticated_ = false;
    bool is_controller_ = false;

    static constexpr size_t MAX_QUEUE_SIZE = 10;
};