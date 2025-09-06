#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <boost/beast/ssl.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class Server;

class HttpSession : public std::enable_shared_from_this<HttpSession> 
{
public:
    HttpSession(
        tcp::socket socket,
        net::ssl::context& ssl_ctx,
        std::shared_ptr<Server> srv, 
        std::string doc_root
    );
    
    void run();
    
private:
    void do_read();
    void handle_request();
    std::string get_mime_type(const std::string& path) const;
    
    net::ssl::stream<tcp::socket> stream_;
    std::shared_ptr<Server> server_;
    std::string doc_root_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
};