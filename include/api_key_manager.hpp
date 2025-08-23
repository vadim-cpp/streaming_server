#pragma once

#include <string>

class APIKeyManager 
{
public:
    static std::string generate_key();
    static bool validate_key(const std::string& key);
};