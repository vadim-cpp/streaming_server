#pragma once

#include "server.hpp"
#include <memory>

class websocket_session; // Предварительное объявление

class http_session : public std::enable_shared_from_this<http_session> 
{
public:
    http_session(tcp::socket socket, std::string doc_root);
    void run();

private:
    void do_read();
    void handle_request();
    std::string get_mime_type(const std::string& path) const;

    beast::tcp_stream socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    std::string doc_root_;
};