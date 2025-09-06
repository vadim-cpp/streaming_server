#include "cloudflare_tunnel.hpp"
#include "logger.hpp"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) 
{
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

bool CloudflareTunnel::is_cloudflared_installed() 
{
    auto logger = Logger::get();
    
    #ifdef _WIN32
    int result = system("where cloudflared > nul 2>&1");
    #else
    int result = system("which cloudflared > /dev/null 2>&1");
    #endif
    
    if (result != 0) 
    {
        logger->warn("cloudflared is not installed or not in PATH");
        return false;
    }
    
    logger->info("cloudflared is available");
    return true;
}

std::string CloudflareTunnel::setup_tunnel(int port) 
{
    auto logger = Logger::get();
    
    if (!is_cloudflared_installed()) 
    {
        return "";
    }
    
    // Проверяем, авторизован ли уже cloudflared
    #ifdef _WIN32
    int auth_status = system("cloudflared tunnel list > nul 2>&1");
    #else
    int auth_status = system("cloudflared tunnel list > /dev/null 2>&1");
    #endif
    
    if (auth_status != 0) 
    {
        logger->warn("cloudflared is not authenticated. Please run: cloudflared login");
        return "";
    }
    
    // Создаем случайное имя для туннеля
    std::string tunnel_name = "ascii-streamer-" + std::to_string(rand() % 10000);
    
    // Создаем туннель
    std::string create_command = "cloudflared tunnel create " + tunnel_name;
    int create_result = system(create_command.c_str());
    
    if (create_result != 0) 
    {
        logger->error("Failed to create Cloudflare tunnel");
        return "";
    }
    
    // Создаем конфигурационный файл
    std::ofstream config_file("config.yml");
    config_file << "tunnel: " << tunnel_name << std::endl;
    config_file << "credentials-file: ./" << tunnel_name << ".json" << std::endl;
    config_file << "ingress:" << std::endl;
    config_file << "  - hostname: " << tunnel_name << ".trycloudflare.com" << std::endl;
    config_file << "    service: http://localhost:" << port << std::endl;
    config_file << "  - service: http_status:404" << std::endl;
    config_file.close();
    
    // Запускаем туннель в фоновом режиме
    std::string run_command = "cloudflared tunnel --config config.yml run " + tunnel_name + " > cloudflared.log 2>&1 &";
    int run_result = system(run_command.c_str());
    
    if (run_result != 0) 
    {
        logger->error("Failed to start Cloudflare tunnel");
        return "";
    }
    
    // Даем время на запуск
    #ifdef _WIN32
    system("timeout 5 > nul");
    #else
    system("sleep 5");
    #endif
    
    // Получаем URL туннеля из логов
    std::ifstream logfile("cloudflared.log");
    std::string line;
    std::string tunnel_url;
    
    while (std::getline(logfile, line)) 
    {
        if (line.find("https://") != std::string::npos) 
        {
            size_t start = line.find("https://");
            size_t end = line.find(".trycloudflare.com");
            if (end != std::string::npos) 
            {
                tunnel_url = line.substr(start, end - start + 18);
                break;
            }
        }
    }
    
    if (tunnel_url.empty()) 
    {
        logger->error("Failed to get tunnel URL from logs");
        return "";
    }
    
    logger->info("Cloudflare tunnel established: {}", tunnel_url);
    return tunnel_url;
}

bool CloudflareTunnel::cleanup() 
{
    // Останавливаем все туннели cloudflared
    #ifdef _WIN32
    system("taskkill /IM cloudflared.exe /F > nul 2>&1");
    #else
    system("pkill cloudflared > /dev/null 2>&1");
    #endif
    
    // Удаляем временные файлы
    #ifdef _WIN32
    system("del /Q cloudflared.log config.yml > nul 2>&1");
    system("del /Q ascii-streamer-*.json > nul 2>&1");
    #else
    system("rm -f cloudflared.log config.yml > /dev/null 2>&1");
    system("rm -f ascii-streamer-*.json > /dev/null 2>&1");
    #endif
    
    return true;
}

bool CloudflareTunnel::is_ngrok_installed() 
{
    auto logger = Logger::get();
    
    #ifdef _WIN32
    int result = system("where ngrok > nul 2>&1");
    #else
    int result = system("which ngrok > /dev/null 2>&1");
    #endif
    
    if (result != 0) 
    {
        logger->warn("ngrok is not installed or not in PATH");
        return false;
    }
    
    logger->info("ngrok is available");
    return true;
}

std::string CloudflareTunnel::setup_ngrok_tunnel(int port) 
{
    auto logger = Logger::get();
    
    if (!is_ngrok_installed()) 
    {
        return "";
    }
    
    // Запускаем ngrok в фоновом режиме
    std::string command = "ngrok http " + std::to_string(port) + " --log=stdout > ngrok.log 2>&1 &";
    int result = system(command.c_str());
    
    if (result != 0) 
    {
        logger->error("Failed to start ngrok tunnel");
        return "";
    }
    
    // Даем время на запуск
    #ifdef _WIN32
    system("timeout 5 > nul");
    #else
    system("sleep 5");
    #endif
    
    // Получаем URL туннеля через API ngrok
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:4040/api/tunnels");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    
    // Парсим JSON ответ
    try 
    {
        size_t pos = response.find("public_url");
        if (pos != std::string::npos) {
            size_t url_start = response.find("https://", pos);
            if (url_start != std::string::npos) {
                size_t url_end = response.find("\"", url_start);
                if (url_end != std::string::npos) {
                    std::string url = response.substr(url_start, url_end - url_start);
                    logger->info("Ngrok tunnel established: {}", url);
                    return url;
                }
            }
        }
    } 
    catch (...) 
    {
        logger->error("Failed to parse ngrok response");
    }
    
    // Если API не сработало, попробуем прочитать из логов
    std::ifstream logfile("ngrok.log");
    std::string line;
    std::string tunnel_url;
    
    while (std::getline(logfile, line)) 
    {
        if (line.find("url=") != std::string::npos)
         {
            size_t start = line.find("url=");
            tunnel_url = line.substr(start + 4);
            tunnel_url = tunnel_url.substr(0, tunnel_url.find(" "));
            break;
        }
    }
    
    if (!tunnel_url.empty()) 
    {
        logger->info("Ngrok tunnel established: {}", tunnel_url);
        return tunnel_url;
    }
    
    logger->error("Failed to get ngrok tunnel URL");
    return "";
}