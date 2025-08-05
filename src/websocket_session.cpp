#include "websocket_session.hpp"
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

websocket_session::websocket_session(beast::tcp_stream stream)
    : ws_(std::move(stream)) {}  // Инициализируем ws_ этим потоком

void websocket_session::run(http::request<http::string_body> req) 
{
    // Устанавливаем таймауты
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server));
    
    // Устанавливаем обработчик для изменения ответа
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "ASCII Stream Server");
        }));
    
    // Принимаем WebSocket соединение
    ws_.async_accept(
        req,
        [self = shared_from_this()](beast::error_code ec) {
            self->on_accept(ec);
        });
}

void websocket_session::on_accept(beast::error_code ec) 
{
    if(ec) 
    {
        std::cerr << "Accept error: " << ec.message() << "\n";
        return;
    }
    do_read();
}

void websocket_session::do_read() 
{
    ws_.async_read(
        buffer_,
        [self = shared_from_this()](beast::error_code ec, size_t bytes) {
            self->on_read(ec, bytes);
        });
}

void websocket_session::on_read(beast::error_code ec, size_t) 
{
    if(ec == websocket::error::closed) 
    {
        return on_close(ec);
    }
    
    if(ec) 
    {
        std::cerr << "Read error: " << ec.message() << "\n";
        return;
    }
    
    // Эхо-ответ
    ws_.text(ws_.got_text());
    ws_.async_write(
        buffer_.data(),
        [self = shared_from_this()](beast::error_code ec, size_t) {
            if(ec) 
            {
                std::cerr << "Write error: " << ec.message() << "\n";
                return;
            }
            
            self->buffer_.consume(self->buffer_.size());
            self->do_read();
        });
}

void websocket_session::on_close(beast::error_code ec) 
{
    if(ec && ec != beast::errc::not_connected) 
    {
        std::cerr << "Close error: " << ec.message() << "\n";
    }
}