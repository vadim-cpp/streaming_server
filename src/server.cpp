#include "server.hpp"
#include "http_session.hpp"
#include "stream_controller.hpp"
#include "logger.hpp"
#include "api_key_manager.hpp"

Server::Server(
    net::io_context& ioc,
    net::ssl::context&& ctx,
    tcp::endpoint endpoint,
    std::string doc_root,
    std::shared_ptr<IVideoSource> video_source,
    std::shared_ptr<IAsciiConverter> ascii_converter
)
    : ioc_(ioc), acceptor_(ioc), doc_root_(std::move(doc_root)),
      ssl_ctx_(std::move(ctx)), video_source_(std::move(video_source)),
      ascii_converter_(std::move(ascii_converter))
{
    // Настройка SSL контекста
    ssl_ctx_.set_options(
        net::ssl::context::default_workarounds |
        net::ssl::context::no_sslv2 |
        net::ssl::context::single_dh_use);

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
    stream_controller_ = std::make_shared<StreamController>(
        ioc, video_source_, ascii_converter_);

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
                
                // Создаем HTTP сессию с socket и SSL контекстом
                std::make_shared<HttpSession>(
                    std::move(socket), 
                    self->ssl_ctx_,
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
    net::io_context& ioc, 
    tcp::endpoint endpoint, 
    std::string doc_root,
    std::shared_ptr<IVideoSource> video_source,
    std::shared_ptr<IAsciiConverter> ascii_converter) 
{
    net::ssl::context ctx{net::ssl::context::tlsv12};

    try 
    {
        ctx.use_certificate_chain_file("server.crt");
        ctx.use_private_key_file("server.key", net::ssl::context::pem);
    } 
    catch (const std::exception& e) 
    {
        auto logger = Logger::get();
        logger->error("Failed to load SSL certificate: {}", e.what());
        throw;
    }

    auto srv = std::make_shared<Server>(ioc, std::move(ctx), endpoint, doc_root, video_source, ascii_converter);
    srv->run();
    return srv;
}