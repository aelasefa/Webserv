#include "../../includes/Socket.hpp"
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <unistd.h>

Socket::Socket(int port) : _port(port), _server_fd(-1) {}

Socket::~Socket() {
    if (_server_fd != -1)
        close(_server_fd);
}

bool Socket::createSocket() {

    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0) {
        std::cerr << "Error: socket creation failed" << std::endl;
        return false;
    }

    std::memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = htonl(INADDR_ANY);
    _address.sin_port = htons(_port);

    return true;
}

bool Socket::setSocketOptions() {

    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error: setsockopt failed\n";
        return false;
    }

    return true;
}

bool Socket::bindSocket() {

    if (bind(_server_fd, (struct sockaddr*)&_address, sizeof(_address)) < 0) {
        std::cerr << "Error: bind failed\n";
        return false;
    }

    return true;
}

bool Socket::listenSocket() {

    if (listen(_server_fd, 10) < 0) {
        std::cerr << "Error: listen failed\n";
        return false;
    }

    return true;
}

int Socket::acceptClient() {

    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Error: accept failed\n";
        return -1;
    }

    return client_fd;
}

void Socket::setNonBlocking(int fd) {

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl non-blocking failed\n";
    }
}

int Socket::getFd() const {
    return _server_fd;
}