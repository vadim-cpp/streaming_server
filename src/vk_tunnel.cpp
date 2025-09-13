#include "vk_tunnel.hpp"
#include "logger.hpp"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) 
{
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

bool VKTunnel::is_vk_tunnel_installed() 
{
    auto logger = Logger::get();
    
    #ifdef _WIN32
    int result = system("where vk-tunnel > nul 2>&1");
    #else
    int result = system("which vk-tunnel > /dev/null 2>&1");
    #endif
    
    if (result != 0) 
    {
        logger->warn("vk-tunnel is not installed or not in PATH");
        return false;
    }
    
    logger->info("vk-tunnel is available");
    return true;
}

std::string VKTunnel::setup_tunnel(int port) 
{
    auto logger = Logger::get();
    
    if (!is_vk_tunnel_installed()) 
    {
        return "";
    }
    
    // Запускаем vk-tunnel в фоновом режиме
    std::string command = "vk-tunnel http " + std::to_string(port) + " --log=stdout > vk_tunnel.log 2>&1 &";
    int result = system(command.c_str());
    
    if (result != 0) 
    {
        logger->error("Failed to start vk-tunnel");
        return "";
    }
    
    // Даем время на запуск
    #ifdef _WIN32
    system("timeout 5 > nul");
    #else
    system("sleep 5");
    #endif
    
    // Пытаемся получить URL из логов
    std::ifstream logfile("vk_tunnel.log");
    std::string line;
    std::string tunnel_url;
    
    while (std::getline(logfile, line)) 
    {
        if (line.find("https://") != std::string::npos) 
        {
            size_t start = line.find("https://");
            size_t end = line.find(".vk-apps.com");
            if (end != std::string::npos) 
            {
                tunnel_url = line.substr(start, end - start + 14);
                break;
            }
        }
    }
    
    if (tunnel_url.empty()) 
    {
        logger->error("Failed to get vk-tunnel URL from logs");
        return "";
    }
    
    logger->info("VK tunnel established: {}", tunnel_url);
    return tunnel_url;
}

bool VKTunnel::cleanup() 
{
    // Останавливаем все процессы vk-tunnel
    #ifdef _WIN32
    system("taskkill /IM vk-tunnel.exe /F > nul 2>&1");
    #else
    system("pkill vk-tunnel > /dev/null 2>&1");
    #endif
    
    // Удаляем временные файлы
    #ifdef _WIN32
    system("del /Q vk_tunnel.log > nul 2>&1");
    #else
    system("rm -f vk_tunnel.log > /dev/null 2>&1");
    #endif
    
    return true;
}