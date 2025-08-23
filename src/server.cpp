#include "server.hpp"
#include "http_session.hpp"
#include "stream_controller.hpp"
#include "logger.hpp"
#include "api_key_manager.hpp"

Server::Server(net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root)
    : ioc_(ioc), acceptor_(ioc), doc_root_(std::move(doc_root)) 
{
    boost::system::error_code ec;
    
    acceptor_.open(endpoint.protocol(), ec);
    if(ec) throw boost::system::system_error(ec);
    
    acceptor_.set_option(net::socket_base::reuse_address(true), ec);
    if(ec) throw boost::system::system_error(ec);
    
    acceptor_.bind(endpoint, ec);
    if(ec) throw boost::system::system_error(ec);
    
    acceptor_.listen(net::socket_base::max_listen_connections, ec);
    if(ec) throw boost::system::system_error(ec);

    api_key_ = APIKeyManager::generate_key();
    stream_controller_ = std::make_shared<StreamController>(ioc);

    auto logger = Logger::get();
    logger->info("Server API key: {}", api_key_);
}

void Server::run() 
{
    do_accept();
}

void Server::do_accept() 
{
    acceptor_.async_accept(
        net::make_strand(ioc_),
        [self = shared_from_this()](boost::system::error_code ec, tcp::socket socket) {
            auto logger = Logger::get();
            if(!ec) 
            {
                logger->info("New connection from: {}", 
                    socket.remote_endpoint().address().to_string());
                std::make_shared<HttpSession>(
                    std::move(socket), 
                    self,
                    self->doc_root_)->run();
            } 
            else 
            {
                logger->error("Accept error: {}", ec.message());
            }
            self->do_accept();
        });
}

std::shared_ptr<Server> make_server(
    net::io_context& ioc, tcp::endpoint endpoint, std::string doc_root) 
{
    auto srv = std::make_shared<Server>(ioc, endpoint, doc_root);
    srv->run();
    return srv;
}