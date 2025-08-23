#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>

namespace net = boost::asio;
using tcp = net::ip::tcp;

class StreamController;

class Server : public std::enable_shared_from_this<Server> 
{
public:
    Server(net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root);
    
    void run();
    tcp::acceptor& acceptor() { return acceptor_; }
    std::string api_key() const { return api_key_; }
    std::shared_ptr<StreamController> stream_controller() { return stream_controller_; }
    
private:
    void do_accept();
    
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::string doc_root_;
    std::string api_key_;
    std::shared_ptr<StreamController> stream_controller_;
};

std::shared_ptr<Server> make_server(net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root);