#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

class server : public std::enable_shared_from_this<server> 
{
public:
    server(net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root);
    void run();

private:
    void do_accept();

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::string doc_root_;
};

std::shared_ptr<server> make_server(
    net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root);