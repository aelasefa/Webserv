#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include "Socket.hpp"
#include <poll.h>
#include <vector>
#include <map>
#include "Client.hpp"
#include "Request.hpp"

class Webserv
{
private:
    int _port;
    Socket _socket;
    std::vector<pollfd> _poll_fds;
    std::map<int, Client> _clients;

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

    Request parseRequest(const std::string &raw);
    int routeRequest(const Request &req, std::string &body);

    int handleGET(const std::string &path, std::string &body);
};

#endif