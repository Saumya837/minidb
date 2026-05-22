#include "server.h"
#include <arpa/inet.h>

uint32_t create_socket(){
    uint32_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1){
              throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno))); 
    }
    return server_fd;
}

bool bind_socket(uint32_t server_fd, const std::string& host, uint32_t port){
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Convert host string to binary IP
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
        return false;   // invalid IP string

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        return false;

    return true;
}
 