#pragma once

#include "stream_controller.hpp"

#include <memory>
#include <deque>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class Server;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> 
{
public:
    WebSocketSession(
        net::ssl::stream<tcp::socket> stream,
        std::shared_ptr<StreamController> controller, 
        std::shared_ptr<Server> server
    );
    ~WebSocketSession();
    
    void run(http::request<http::string_body> req);
    void send_frame(const std::string& frame);
    void close();

    uint64_t session_id() const { return session_id_; }

private:
    net::awaitable<void> do_run(http::request<http::string_body> req);
    net::awaitable<void> do_read();
    net::awaitable<void> handle_message(const std::string& message);
    net::awaitable<void> do_write();

    size_t get_queue_size() const { return write_queue_.size(); }
    bool is_authenticated() const { return is_authenticated_; }
    bool is_controller() const { return is_controller_; }
    uint64_t generate_session_id();
    
    websocket::stream<net::ssl::stream<tcp::socket>> ws_;
    std::shared_ptr<StreamController> controller_;
    std::shared_ptr<Server> server_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
    bool is_writing_ = false;
    bool is_authenticated_ = false;
    bool is_controller_ = false;
    uint64_t session_id_;

    static constexpr size_t MAX_QUEUE_SIZE = 10;
};