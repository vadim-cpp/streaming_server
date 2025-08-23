#include "server.hpp"
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
        const std::string doc_root = "../../web";
        
        // Запуск сервера
        net::io_context ioc;
        auto server = make_server(ioc, tcp::endpoint(
            net::ip::make_address(address), port), doc_root);
        
        logger->info("Server listening on {}:{}", address, port);
        ioc.run();
    } 
    catch (const std::exception& e) 
    {
        logger->critical("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}