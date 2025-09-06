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

std::string APIKeyManager::generate_invite_token(const std::string& api_key, int duration_minutes) 
{
    auto now = std::chrono::system_clock::now();
    auto expiry = now + std::chrono::minutes(duration_minutes);
    auto expiry_time_t = std::chrono::system_clock::to_time_t(expiry);
    
    std::stringstream ss;
    ss << api_key << "|" << expiry_time_t;
    std::string token_data = ss.str();
    
    // TODO: Простое "шифрование"
    for (char& c : token_data) 
    {
        c = c + 3;
    }
    
    return base64_encode(token_data);
}

bool APIKeyManager::validate_invite_token(const std::string& token, std::string& api_key) 
{
    try 
    {
        std::string decoded = base64_decode(token);
        
        // TODO: Расшифровка
        for (char& c : decoded) 
        {
            c = c - 3;
        }
        
        size_t pos = decoded.find('|');
        if (pos == std::string::npos) 
        {
            return false;
        }
        
        api_key = decoded.substr(0, pos);
        std::string expiry_str = decoded.substr(pos + 1);
        std::time_t expiry_time = std::stoll(expiry_str);
        
        auto now = std::chrono::system_clock::now();
        auto expiry = std::chrono::system_clock::from_time_t(expiry_time);
        
        if (now > expiry) 
        {
            return false;
        }
        
        return validate_key(api_key);
    } 
    catch (...) 
    {
        return false;
    }
}

std::string APIKeyManager::base64_encode(const std::string& input) 
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t in_len = input.size();
    const char* bytes_to_encode = input.c_str();
    
    while (in_len--) 
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) 
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) 
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';
        
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];
        
        while(i++ < 3)
            ret += '=';
    }
    
    return ret;
}

std::string APIKeyManager::base64_decode(const std::string& input) 
{
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    size_t in_len = input.size();
    
    while (in_len-- && (input[in_] != '=')) 
    {
        char_array_4[i++] = input[in_]; in_++;
        if (i == 4) 
        {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
            
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; i < 3; i++)
                ret += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) 
    {
        for (j = i; j < 4; j++)
            char_array_4[j] = 0;
        
        for (j = 0; j < 4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
        
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; j < i - 1; j++)
            ret += char_array_3[j];
    }
    
    return ret;
}