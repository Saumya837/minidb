#pragma once

#include <netinet/in.h>
#include <sys/types.h>
#include <string>

namespace minidb {

    struct ServerConfig {
        std::string host;
        uint32_t port;
        uint32_t backlog;
        uint32_t max_connections;
    };

    struct Connection{
        uint32_t fd;
        pid_t backend_pid;
        struct sockaddr_in addr;
    };

    struct Server{
        uint32_t server_fd;
        ServerConfig config;
    };

    int create_socket();
    bool bind_socket(uint32_t server_fd, const std::string& host, uint32_t port);
    bool start_listening(uint32_t server_fd, uint32_t backlog);
    Connection accept_connection(uint32_t server_fd);
    pid_t fork_backend(const Connection& conn);

    Server server_init(const ServerConfig& config);
    void server_run(Server& server);
    void server_shutdown(Server& server);

}

