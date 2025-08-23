#include "api_key_manager.hpp"

#include <random>
#include <algorithm>
#include <chrono>

std::string APIKeyManager::generate_key() 
{
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);
    
    std::string key;
    for (int i = 0; i < 32; ++i) 
    {
        key += chars[distribution(generator)];
    }
    
    return key;
}

bool APIKeyManager::validate_key(const std::string& key) 
{
    return key.length() == 32;
}