#pragma once

#include <string>
#include <random>
#include <ctime>
#include <array>

class APIKeyManager 
{
public:
    static std::string generate_key() 
    {
        static const std::string chars = 
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        
        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> dist(0, chars.size() - 1);
        
        std::string key;
        key.reserve(32);
        
        for (int i = 0; i < 32; ++i) 
        {
            key += chars[dist(generator)];
        }
        
        return key;
    }
};