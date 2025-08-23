#include "http_session.hpp"
#include "websocket_session.hpp"
#include "server.hpp"
#include "video_source.hpp"
#include "logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

HttpSession::HttpSession(tcp::socket socket, std::shared_ptr<Server> srv, std::string doc_root)
    : stream_(std::move(socket)), server_(srv), doc_root_(std::move(doc_root)) {}

void HttpSession::run() 
{
    net::dispatch(stream_.get_executor(),
        [self = shared_from_this()] {
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

    return "application/octet-stream";
}

void HttpSession::handle_request() 
{
    auto logger = Logger::get();
    logger->debug("HTTP request: {}", request_.target());

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
        
        http::response<http::string_body> res;
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.body() = j.dump();
        
        res.prepare_payload();
        http::write(stream_, res);
        return;
    }

    if (request_.target() == "/api") 
    {
        auto logger = Logger::get();
        logger->debug("Handling /api request");
        
        http::response<http::string_body> res;
        res.result(http::status::ok);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        
        nlohmann::json j;
        j["api_key"] = server_->api_key();
        
        auto endpoint = server_->acceptor().local_endpoint();
        std::string address = endpoint.address().to_string();
        unsigned short port = endpoint.port();
        
        if (address == "0.0.0.0") address = "localhost";
        
        j["endpoint"] = "ws://" + address + ":" + std::to_string(port) + "/stream";
        
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
    
    http::response<http::string_body> res;
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