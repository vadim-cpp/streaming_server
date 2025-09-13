#pragma once

#include <string>

class VKTunnel 
{
public:
    static bool is_vk_tunnel_installed();
    static std::string setup_tunnel(int port);
    static bool cleanup();
};