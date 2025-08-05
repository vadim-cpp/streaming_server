#include "http_session.hpp"
#include "websocket_session.hpp"
#include <boost/beast/websocket.hpp>
#include <fstream>
#include <sstream>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

http_session::http_session(tcp::socket socket, std::string doc_root)
    : socket_(std::move(socket)), doc_root_(std::move(doc_root)) {}

void http_session::run() 
{
    net::dispatch(socket_.get_executor(),
        [self = shared_from_this()] {
            self->do_read();
        });
}

void http_session::do_read() 
{
    request_ = {};
    http::async_read(socket_, buffer_, request_,
        [self = shared_from_this()](beast::error_code ec, size_t bytes) {
            if(ec) return;
            
            if(beast::websocket::is_upgrade(self->request_)) 
            {
                // Создаем tcp_stream напрямую без указания версии
                auto stream = beast::tcp_stream(std::move(self->socket_));
                auto ws_session = std::make_shared<websocket_session>(std::move(stream));
                ws_session->run(self->request_);
                return;
            }
            
            self->handle_request();
        });
}

std::string http_session::get_mime_type(const std::string& path) const 
{
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".html") == 0)
        return "text/html";

    if (path.size() > 3 && path.compare(path.size() - 3, 3, ".js") == 0)
        return "application/javascript";

    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".css") == 0)
        return "text/css";

    return "application/octet-stream";
}

void http_session::handle_request() 
{
    std::string path = doc_root_;
    path.append(request_.target().data(), request_.target().size());
    
    if (path.back() == '/') 
    {
        path += "index.html";
    }
    
    http::response<http::string_body> res;
    std::ifstream file(path, std::ios::binary);
    
    if (file) 
    {
        std::ostringstream ss;
        ss << file.rdbuf();
        res.body() = ss.str();
        res.result(http::status::ok);
        res.set(http::field::content_type, get_mime_type(path));
    } 
    else 
    {
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "File not found: " + path;
    }
    
    res.prepare_payload();
    http::write(socket_, res);
}