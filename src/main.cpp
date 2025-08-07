#include "server.hpp"
#include <iostream>

int main() 
{
    try 
    {
        // Конфигурация сервера
        const std::string address = "0.0.0.0";
        const unsigned short port = 8080;
        const std::string doc_root = "../web";
        
        // Запуск сервера
        net::io_context ioc;
        auto server = make_server(ioc, tcp::endpoint(
            net::ip::make_address(address), port), doc_root);
        
        std::cout << "Server listening on " << address << ":" << port << "\n";
        ioc.run();
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}