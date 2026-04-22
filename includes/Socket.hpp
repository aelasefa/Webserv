#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>

class Socket
{
private:
    int _port;
    int _server_fd;
    sockaddr_in _address;

public:
    Socket(int port);
    ~Socket();

    bool createSocket();
    bool setSocketOptions();
    bool bindSocket();
    bool listenSocket();

    int acceptClient();

    void setNonBlocking(int fd);

    int getFd() const;
};

#endif