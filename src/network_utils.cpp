#include "network_utils.hpp"
#include <curl/curl.h>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) 
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string get_local_ip() 
{
    std::string ip;

#ifdef _WIN32
    // Реализация для Windows
    PIP_ADAPTER_ADDRESSES addresses = NULL;
    ULONG outBufLen = 0;
    DWORD result = 0;

    // Получаем размер буфера
    result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &outBufLen);
    if (result == ERROR_BUFFER_OVERFLOW) 
    {
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(outBufLen);
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, addresses, &outBufLen);
    }

    if (result == NO_ERROR) 
    {
        PIP_ADAPTER_ADDRESSES currAddress = addresses;
        while (currAddress) {
            // Пропускаем loopback-интерфейсы и неактивные адаптеры
            if (currAddress->OperStatus == IfOperStatusUp && 
                currAddress->IfType != IF_TYPE_SOFTWARE_LOOPBACK) 
                {
                
                PIP_ADAPTER_UNICAST_ADDRESS unicastAddress = currAddress->FirstUnicastAddress;
                while (unicastAddress) 
                {
                    if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET) 
                    {
                        sockaddr_in* sockaddr = (sockaddr_in*)unicastAddress->Address.lpSockaddr;
                        char buffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(sockaddr->sin_addr), buffer, INET_ADDRSTRLEN);
                        ip = buffer;
                        
                        // Предпочитаем нелокальные адреса
                        if (ip != "127.0.0.1" && ip.substr(0, 3) != "169") 
                        {
                            free(addresses);
                            return ip;
                        }
                    }
                    unicastAddress = unicastAddress->Next;
                }
            }
            currAddress = currAddress->Next;
        }
    }
    
    if (addresses) 
    {
        free(addresses);
    }
#else
    // Реализация для Linux
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) 
    {
        return "127.0.0.1";
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
    {
        if (ifa->ifa_addr == NULL) continue;
        
        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) 
        {
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                               host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s == 0) 
            {
                std::string interface_name(ifa->ifa_name);
                // Исключаем loopback и виртуальные интерфейсы
                if (interface_name != "lo" && 
                    interface_name.find("docker") == std::string::npos &&
                    interface_name.find("veth") == std::string::npos) {
                    ip = host;
                    break;
                }
            }
        }
    }
    
    freeifaddrs(ifaddr);
#endif

    return ip.empty() ? "127.0.0.1" : ip;
}

bool is_port_open(const std::string& ip, int port) 
{
#ifdef _WIN32
    // Реализация для Windows
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
    {
        return false;
    }
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) 
    {
        WSACleanup();
        return false;
    }
    
    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);
    
    // Устанавливаем таймаут
    DWORD timeout = 3000; // 3 секунды
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    
    bool result = (connect(sock, (sockaddr*)&server, sizeof(server)) >= 0);
    closesocket(sock);
    WSACleanup();
    return result;
#else
    // Реализация для Linux
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server.sin_addr);
    
    // Устанавливаем таймаут
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    
    bool result = (connect(sock, (struct sockaddr*)&server, sizeof(server)) >= 0);
    close(sock);
    return result;
#endif
}