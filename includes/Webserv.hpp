#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <cstddef>
#include <iostream>
#include <poll.h>
#include <vector>
#include <map>

#include "Client.hpp"
#include "Server.hpp"
#include "SessionManager.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGI.hpp"

class Webserv
{
private:
    std::vector<Server> _servers;
    std::vector<pollfd> _poll_fds;
    std::vector<int> _server_fds;
    std::map<int, Client> _clients;
    SessionManager _sessionManager;

    void addServerToPoll(int server_fd);
    void addClientToPoll(int fd);
    void removeClientAt(size_t index);
    void handleNewConnection(int server_fd);
    void handleClientRead(size_t index);
    void handleClientWrite(size_t index);
    bool processClientRequest(Client &client, Request &req, pollfd &pfd);
    void pollRunningCgis();
    bool isServerFd(int fd) const;
    Response buildErrorResponse(const Server &server, int code);

    const Server &getServerByIndex(size_t index) const;

public:
    Webserv(const std::vector<Server> &servers);
    ~Webserv();

    void initServers();
    // [FIX LOW-4] Removed dead declarations: checkTimeouts() and getServerByIndex()
    // had no implementation bodies, causing potential linker errors if called.
    void setNonBlocking(int fd);
    void startLoop();
};

#endif