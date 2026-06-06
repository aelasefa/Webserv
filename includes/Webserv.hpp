#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <cstddef>
#include <iostream>
#include <poll.h>
#include <vector>
#include <map>
#include "Client.hpp"
#include "Server.hpp"

class Webserv
{
private:
    std::vector<Server> _servers;
    std::vector<pollfd> _poll_fds;
    std::vector<int> _server_fds;
    std::map<int, Client> _clients;

    void addServerToPoll(int server_fd);
    void addClientToPoll(int fd);
    void removeClientAt(size_t index);
    void handleNewConnection(int server_fd);
    void handleClientRead(size_t index);
    void handleClientWrite(size_t index);
    bool processClientRequest(Client &client, Request &req, pollfd &pfd);
    bool isServerFd(int fd) const;

public:
    Webserv(const std::vector<Server> &servers);
    ~Webserv();

    void initServers();
    void startLoop();

};

#endif