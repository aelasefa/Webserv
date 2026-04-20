#include "../includes/Webserv.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <cstdlib>

Webserv::Webserv(int port) : _port(port), _server_fd(-1) {}

Webserv::~Webserv()
{
    if (_server_fd != -1)
        close(_server_fd);
}

void Webserv::initServer()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < -1)
    {
        std::cerr << "Eroor: socket creation failed..." << std::endl;
        exit(1);
    }
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::cerr << "Error: setsockopt failed\n";
        exit(1);
    }
    std::memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = htonl(INADDR_ANY);
    _address.sin_port = htons(_port);
}
void Webserv::bindServer()
{
    if (bind(_server_fd, (struct sockaddr*)&_address, sizeof(_address)) < 0) {
        std::cerr << "Error: bind failed\n";
        exit(1);
    }
}
void Webserv::startListening()
{
    if (listen(_server_fd, 10) < 0) {
        std::cerr << "Error: listen failed\n";
        exit(1);
    }
    std::cout << "Server listening on port " << _port << std::endl;
}

void Webserv::startLoop() {
        while (true) {
        int client_fd = acceptClient();
        if (client_fd < 0)
            continue;

        char buffer[1024];
        std::memset(buffer, 0, sizeof(buffer));

        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            std::cout << "Request:\n" << buffer << std::endl;
        }

        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 5\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Hello";

        send(client_fd, response, strlen(response), 0);

        close(client_fd);
    }
}

int Webserv::acceptClient()
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Error: accept failed\n";
        return -1;
    }

    std::cout << "Client connected: fd = " << client_fd << std::endl;
    return client_fd;
}

int Webserv::getServerFD() const
{
    return _server_fd;
}
