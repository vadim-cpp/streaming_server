#pragma once

#include <string>
#include <chrono>

class APIKeyManager 
{
public:
    static std::string generate_key();
    static bool validate_key(const std::string& key);

    static std::string generate_invite_token(const std::string& api_key, int duration_minutes);
    static bool validate_invite_token(const std::string& token, std::string& api_key);

private:
    static std::string base64_encode(const std::string& input);
    static std::string base64_decode(const std::string& input);
};