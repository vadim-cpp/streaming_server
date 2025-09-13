#include "http_session.hpp"
#include "websocket_session.hpp"
#include "server.hpp"
#include "video_source.hpp"
#include "logger.hpp"
#include "network_utils.hpp"
#include "api_key_manager.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

HttpSession::HttpSession(
    tcp::socket socket,
    net::ssl::context& ssl_ctx,
    std::shared_ptr<Server> srv, 
    std::string doc_root
)
    : stream_(std::move(socket), ssl_ctx),
    server_(srv), 
    doc_root_(std::move(doc_root)) 
{}

void HttpSession::run() 
{
    auto self = shared_from_this();
    stream_.async_handshake(
        boost::asio::ssl::stream_base::server,
        [self](boost::system::error_code ec) {
            if(ec) 
            {
                auto logger = Logger::get();
                logger->error("SSL handshake failed: {} (category: {})", 
                                 ec.message(), ec.category().name());
                return;
            }
            self->do_read();
        });
}

void HttpSession::do_read() 
{
    auto logger = Logger::get();

    request_ = {};
    http::async_read(stream_, buffer_, request_,
        [self = shared_from_this(), logger](beast::error_code ec, size_t bytes) {
            if(ec) 
            {
                logger->warn("HTTP read error: {}", ec.message());
                return;
            }

            if(beast::websocket::is_upgrade(self->request_)) 
            {
                logger->debug("WebSocket upgrade requested");
                
                // Создаем WebSocket сессию с SSL потоком
                auto ws_session = std::make_shared<WebSocketSession>(
                    std::move(self->stream_),
                    self->server_->stream_controller(),
                    self->server_);
                    
                ws_session->run(self->request_);
                return;
            }
            
            self->handle_request();
        });
}

std::string HttpSession::get_mime_type(const std::string& path) const 
{
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".html") == 0)
        return "text/html";

    if (path.size() > 3 && path.compare(path.size() - 3, 3, ".js") == 0)
        return "application/javascript";

    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".css") == 0)
        return "text/css";

    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".png") == 0)
        return "image/png";

    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".jpg") == 0)
        return "image/jpeg";

    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".jpeg") == 0)
        return "image/jpeg";

    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".ico") == 0)
        return "image/x-icon";

    return "application/octet-stream";
}

void HttpSession::handle_request() 
{
    auto logger = Logger::get();
    logger->debug("HTTP request: {}", request_.target());

    http::response<http::string_body> res;
    
    // Устанавливаем CORS заголовки для всех ответов
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
    res.set(http::field::access_control_max_age, "86400");

    // Обработка предварительного OPTIONS запроса
    if (request_.method() == http::verb::options) 
    {
        logger->debug("Handling OPTIONS request for CORS");
        res.result(http::status::ok);
        res.content_length(0);
        http::write(stream_, res);
        return;
    }

    std::string path = doc_root_;
    path.append(request_.target().data(), request_.target().size());

    if (request_.target() == "/cameras") 
    {
        logger->debug("Handling /cameras request");
        auto cameras = VideoSource::list_cameras();
        
        nlohmann::json j;
        for (const auto& camera : cameras) 
        {
            j.push_back({
                {"index", camera.index},
                {"name", camera.name}
            });
        }
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = j.dump();
        
        res.prepare_payload();
        http::write(stream_, res);
        return;
    }

    if (request_.target() == "/api") 
    {
        logger->debug("Handling /api request");
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        
        nlohmann::json j;
        j["api_key"] = server_->api_key();
        
        auto endpoint = server_->acceptor().local_endpoint();
        std::string tunnel_url = server_->cloud_tunnel_url();
        unsigned short port = endpoint.port();
        std::string address;

        if (!tunnel_url.empty())
        {
            address = server_->cloud_tunnel_url();
            size_t pos = address.find("https://");
            if (pos != std::string::npos)
            {
                size_t start = address.find("https://");
                address.erase(pos, 8);
            }
            j["endpoint"] = "wss://" + address + "stream";
        }
        else
        {
            address = get_local_ip();

            j["endpoint"] = "wss://" + address + ":" + std::to_string(port) + "/stream";
        }
        
        res.body() = j.dump();
        res.prepare_payload();
        http::write(stream_, res);
        return;
    }

    if (request_.target() == "/tunnel_status") 
    {
        nlohmann::json j;
        
        std::string tunnel_url = server_->cloud_tunnel_url();
        j["available"] = !tunnel_url.empty();
        j["url"] = tunnel_url;
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = j.dump();
        
        res.prepare_payload();
        http::write(stream_, res);
        return;
    }

    if (request_.target() == "/test_tunnel") 
    {
        nlohmann::json j;
        
        std::string tunnel_url = server_->cloud_tunnel_url();

        if (!tunnel_url.empty())
        {
            j["success"] = true;
            j["message"] = "Tunnel is accessible";
        }
        else 
        {
            j["success"] = false;
            j["message"] = "No tunnel available";
        }
        
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = j.dump();
        
        res.prepare_payload();
        http::write(stream_, res);
        return;
    }
    
    if (path.back() == '/') 
    {
        logger->debug("Appending index.html to path");
        path += "index.html";
    }
    
    logger->debug("Opening file: {}", path);
    std::ifstream file(path, std::ios::binary);
    
    if (file) 
    {
        logger->info("Serving file: {}", path);

        std::ostringstream ss;
        ss << file.rdbuf();
        res.body() = ss.str();
        res.result(http::status::ok);
        res.set(http::field::content_type, get_mime_type(path));
    } 
    else 
    {
        logger->warn("File not found: {}", path);
        
        res.result(http::status::not_found);
        res.set(http::field::content_type, "text/plain");
        res.body() = "File not found: " + path;
    }
    
    logger->debug("Sending HTTP response");
    res.prepare_payload();
    http::write(stream_, res);
}