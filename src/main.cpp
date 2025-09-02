#include "server.hpp"
#include "video_source.hpp"
#include "ascii_converter.hpp"
#include "logger.hpp"

#include <iostream>

int main() 
{
    Logger::init();
    auto logger = Logger::get();

    try 
    {
        logger->info("Starting ASCII streamer server");

        // Конфигурация сервера
        const std::string address = "0.0.0.0";
        const unsigned short port = 8080;
        const std::string doc_root = "../web";

        auto video_source = std::make_shared<VideoSource>();
        auto ascii_converter = std::make_shared<AsciiConverter>();
        
        // Запуск сервера
        net::io_context ioc;
        auto server = make_server(ioc, tcp::endpoint(
            net::ip::make_address(address), port), doc_root, video_source, ascii_converter);
        
        logger->info("Server listening on {}:{}", address, port);
        logger->info("SSL/TLS enabled - using HTTPS/WSS protocol");
        ioc.run();
    } 
    catch (const std::exception& e) 
    {
        logger->critical("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}