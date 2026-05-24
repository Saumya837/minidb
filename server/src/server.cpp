#include "server.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>


namespace minidb {
        
    Server server_init(const ServerConfig& config) {
        Server server;
        server.config = config;
        server.server_fd = create_socket();
        if (!bind_socket(server.server_fd, config.host, config.port)) {
            throw std::runtime_error("Failed to bind socket");
        }
        start_listening(server.server_fd, config.backlog);
        return server;
    }

    void server_run(Server& server) {
        while(true){
            Connection conn = accept_connection(server.server_fd);
            fork_backend(conn);
        }
    }

    void server_shutdown(Server& server) {
        if(server.server_fd >= 0){
            close(server.server_fd);
            server.server_fd = -1;
        }
    }

    int create_socket(){
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1){
            throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno))); 
        }
        return server_fd;
    }

    bool bind_socket(int server_fd, const std::string& host, uint32_t port){
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (host.empty() || host == "0.0.0.0")
            addr.sin_addr.s_addr = INADDR_ANY;
        // Convert host string to binary IP
        else if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0)
            return false;   // invalid IP string

        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("bind failed: " + std::string(strerror(errno)));

        return true;
    }

    bool start_listening(int server_fd, uint32_t backlog){
        if(listen(server_fd, backlog) < 0)
            throw std::runtime_error("listen failed:" + std::string(std::strerror(errno)));
        return true;
    }

    Connection accept_connection(int server_fd){
        Connection conn{};
        socklen_t addr_len = sizeof(conn.addr);
        conn.fd = accept(server_fd, (sockaddr*)&conn.addr, &addr_len);
        if (conn.fd < 0) {
            throw std::runtime_error("accep failed: " + std::string(strerror(errno)));
        }
        return conn;
    }

    pid_t fork_backend(const Connection& conn) {
        pid_t pid = fork();
        if (pid < 0)
            throw std::runtime_error("fork failed: " + std::string(strerror(errno)));
        
        if (pid == 0) {
            // child process — handle client
            // handle connection, read queries, execute them
            char buf[1024];
            ssize_t bytes= recv(conn.fd, buf, sizeof(buf) - 1, 0);

            if(bytes > 0){
                buf[bytes] = '\0';
                // -- Future Integration: parse query and execute
                std::cout<< "Recived Query: " << buf << std::endl;
            }
            
            close(conn.fd);
            exit(0);
        }
        
        // parent process — close client fd, return child pid
        close(conn.fd);
        return pid;
    }
}
 