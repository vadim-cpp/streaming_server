#include "websocket_session.hpp"
#include "server.hpp"
#include "logger.hpp"

#include <nlohmann/json.hpp>

WebSocketSession::WebSocketSession(beast::tcp_stream stream, 
                                   std::shared_ptr<StreamController> controller, 
                                   std::shared_ptr<Server> server)
    : ws_(std::move(stream)), 
      controller_(controller), 
      server_(server) 
{}

WebSocketSession::~WebSocketSession() 
{
    auto logger = Logger::get();
    
    if (is_controller_) 
    {
        net::co_spawn(ws_.get_executor(),
            [controller = controller_] { return controller->stop_streaming(); },
            net::detached);
    } 
    else if (is_authenticated_) 
    {
        net::co_spawn(ws_.get_executor(),
            [controller = controller_, self = shared_from_this()] { 
                return controller->remove_viewer(self); 
            },
            net::detached);
    }
    
    logger->debug("WebSocket session destroyed");
}

void WebSocketSession::run(http::request<http::string_body> req) 
{
    net::co_spawn(
        ws_.get_executor(),
        [self = shared_from_this(), req = std::move(req)]() mutable {
            return self->do_run(std::move(req));
        },
        net::detached);
}

void WebSocketSession::send_frame(const std::string& frame) 
{
    // Создаем shared_ptr для строки, чтобы гарантировать ее время жизни
    auto frame_ptr = std::make_shared<std::string>(frame);
    
    net::post(ws_.get_executor(),
        [self = shared_from_this(), frame_ptr] {
            if (!self->ws_.is_open()) return;

            if (self->write_queue_.size() >= MAX_QUEUE_SIZE) 
            {
                self->write_queue_.pop_front();
            }
            
            self->write_queue_.push_back(*frame_ptr);
            
            if (!self->is_writing_) 
            {
                self->is_writing_ = true;
                net::co_spawn(self->ws_.get_executor(),
                    [self] { return self->do_write(); },
                    net::detached);
            }
        });
}

void WebSocketSession::close() 
{
    if (ws_.is_open()) 
    {
        ws_.async_close(websocket::close_code::normal,
            [self = shared_from_this()](beast::error_code ec) {
                if (ec) 
                {
                    auto logger = Logger::get();
                    logger->error("WebSocket close error: {}", ec.message());
                }
            });
    }
}

net::awaitable<void> WebSocketSession::do_write()
{
    auto logger = Logger::get();
    try {
        while (!write_queue_.empty() && ws_.is_open())
        {
            std::string frame = std::move(write_queue_.front());
            write_queue_.pop_front();

            co_await ws_.async_write(net::buffer(frame), net::use_awaitable);
        }
    }
    catch (const beast::system_error& e) 
    {
        // Игнорируем ошибки, связанные с закрытием соединения
        if (e.code() == net::error::operation_aborted || 
            e.code() == websocket::error::closed ||
            e.code() == net::error::eof ||
            e.code() == net::error::connection_reset ||
            e.code() == net::error::connection_aborted) 
        {
            logger->debug("Write aborted due to closed connection: {}", e.what());
        } 
        else 
        {
            logger->error("Write error: {}", e.what());
            close();
        }
    }
    catch (const std::exception& e) 
    {
        if (ws_.is_open()) 
        {
            logger->error("Write error: {}", e.what());
            close();
        } 
        else 
        {
            logger->debug("Write error on closed socket: {}", e.what());
        }
    }
    is_writing_ = false;
}

net::awaitable<void> WebSocketSession::do_run(http::request<http::string_body> req) 
{
    auto logger = Logger::get();
    
    try 
    {
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
            beast::role_type::server));

        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(http::field::server, "ASCII Stream Server");
            }));
        
        co_await ws_.async_accept(req, net::use_awaitable);
        logger->info("WebSocket connection established");
        
        while (ws_.is_open()) 
        {
            try
            {
                co_await do_read();
            }
            catch (const beast::system_error& e) 
            {
                // Игнорируем ошибки, связанные с закрытием соединения
                if (e.code() == websocket::error::closed ||
                    e.code() == net::error::eof ||
                    e.code() == net::error::connection_reset ||
                    e.code() == net::error::connection_aborted) 
                {
                    logger->info("WebSocket connection closed by client");
                    break;
                }

                if (ws_.is_open()) 
                {
                    close();
                }
                logger->error("WebSocket error: {}", e.what());
                break;
            }
            catch (const std::exception& e) 
            {
                logger->error("Error in do_run: {}", e.what());
                break;
            }
        }
    } 
    catch (const std::exception& e) 
    {
        // Игнорируем ошибки, связанные с закрытием соединения
        if (strstr(e.what(), "closed") == nullptr &&
            strstr(e.what(), "The WebSocket stream was gracefully closed") == nullptr &&
            strstr(e.what(), "Connection reset") == nullptr &&
            strstr(e.what(), "Connection aborted") == nullptr) 
        {
            logger->error("WebSocket error: {}", e.what());
        } 
        else 
        {
            logger->info("WebSocket connection closed by client");
        }
    }
}

net::awaitable<void> WebSocketSession::do_read() 
{
    auto logger = Logger::get();
    
    try 
    {
        buffer_.clear();
        co_await ws_.async_read(buffer_, net::use_awaitable);
        
        auto message = beast::buffers_to_string(buffer_.data());
        logger->debug("Received message: {}", message);
        
        co_await handle_message(message);
    } 
    catch (const beast::system_error& e) 
    {
        if (e.code() != websocket::error::closed && 
            e.code() != net::error::eof &&
            e.code() != net::error::connection_reset &&
            e.code() != net::error::connection_aborted) 
        {
            logger->error("WebSocket read error: {}", e.what());
        }
        else
        {
            logger->debug("WebSocket connection closed by client");
        }
        throw;
    }
}

net::awaitable<void> WebSocketSession::handle_message(const std::string& message) 
{
    auto logger = Logger::get();
    
    try 
    {
        auto j = nlohmann::json::parse(message);
        std::string type = j["type"];
        
        if (type == "auth") 
        {
            std::string api_key = j["api_key"];
            std::string role = j.value("role", "viewer");
            
            if (api_key != server_->api_key()) 
            {
                co_await ws_.async_write(net::buffer("AUTH_FAILED"), net::use_awaitable);
                co_return;
            }
            
            is_authenticated_ = true;
            
            if (role == "controller") 
            {
                is_controller_ = true;
                controller_->add_viewer(shared_from_this());
                send_frame("AUTH_CONTROLLER_SUCCESS");
            } 
            else 
            {
                controller_->add_viewer(shared_from_this());
                send_frame("AUTH_VIEWER_SUCCESS");
                
                std::string status = controller_->is_streaming() ? "STREAM_ACTIVE" : "STREAM_INACTIVE";
                send_frame(status);
            }
        } 
        else if (type == "config" && is_controller_) 
        {
            int camera_index = j.value("camera_index", 0);
            std::string resolution = j.value("resolution", "120x90");
            int fps = j.value("fps", 10);
            
            co_await controller_->start_streaming(camera_index, resolution, fps);
            send_frame("CONFIG_APPLIED");
        } 
        else if (type == "stop" && is_controller_) 
        {
            co_await controller_->stop_streaming();
            send_frame("STREAM_STOPPED");
        } 
        else 
        {
            send_frame("UNKNOWN_COMMAND");
        }
    } 
    catch (const std::exception& e) 
    {
        logger->error("Message handling error: {}", e.what());
        std::string error_msg = "ERROR: " + std::string(e.what());
        send_frame(error_msg);
    }
}