#pragma once

#include <string>

class CloudflareTunnel 
{
public:
    static bool is_cloudflared_installed();
    static std::string setup_tunnel(int port);
    static bool cleanup();

    static bool is_ngrok_installed();
    static std::string setup_ngrok_tunnel(int port);
};