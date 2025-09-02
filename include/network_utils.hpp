#pragma once

#include <string>

std::string get_local_ip();
std::string get_public_ip();
bool is_port_open(const std::string& ip, int port);
