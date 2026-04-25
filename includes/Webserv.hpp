#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include "Socket.hpp"
#include <poll.h>
#include <vector>

class Webserv
{
private:
    int _port;
    Socket _socket;
    std::vector<pollfd> _poll_fds;

public:
    Webserv(int port);
    ~Webserv();

    void initServer();
    void startLoop();

    void addServerToPoll();
    void addClientToPoll(int fd);
    void removeClient(int index);

    void handleNewConnection();
    void handleClientData(int index);
};

#endif