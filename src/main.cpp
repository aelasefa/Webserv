#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>

#include "../includes/Client.hpp"
#include "../includes/Request.hpp"
#include "../includes/MethodHandler.hpp"
#include "../includes/Response.hpp"
#include "../includes/ConfigParser.hpp"

#define MAX_EVENTS 1024

// ---------------- NON BLOCKING ----------------
int setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// ---------------- CREATE SERVER SOCKET ----------------
int createServerSocket(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        throw std::runtime_error("socket failed");

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind failed");

    if (listen(server_fd, SOMAXCONN) < 0)
        throw std::runtime_error("listen failed");

    setNonBlocking(server_fd);

    std::cout << "[SERVER] Listening on port " << port << std::endl;

    return server_fd;
}

// ---------------- MAIN ----------------
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <config_file>\n";
        return 1;
    }

    // ---------------- LOAD CONFIG ----------------
    ConfigParser parser;
    Config config = parser.parse(argv[1]);

    const std::vector<Server> &servers = config.servers;
    if (servers.empty())
    {
        std::cerr << "Error: no server defined in config\n";
        return 1;
    }

    // ---------------- CREATE SERVER SOCKETS ----------------
    std::vector<pollfd> fds;
    std::vector<int> server_fds;
    std::map<int, Client> clients;

    for (size_t i = 0; i < servers.size(); i++)
    {
        int server_fd = createServerSocket(servers[i].listen);
        pollfd server_pfd;
        server_pfd.fd = server_fd;
        server_pfd.events = POLLIN;
        server_pfd.revents = 0;
        fds.push_back(server_pfd);
        server_fds.push_back(server_fd);
    }

    // ---------------- EVENT LOOP ----------------
    while (true)
    {
        if (poll(&fds[0], fds.size(), -1) < 0)
        {
            perror("poll");
            break;
        }

        for (size_t i = 0; i < fds.size(); i++)
        {
            // ---------- NEW CONNECTION ----------
            if (fds[i].revents & POLLIN)
            {
                // check if server socket
                bool isServer = false;
                for (size_t s = 0; s < server_fds.size(); s++)
                {
                    if (fds[i].fd == server_fds[s])
                    {
                        isServer = true;
                        break;
                    }
                }

                if (isServer)
                {
                    int client_fd = accept(fds[i].fd, NULL, NULL);
                    if (client_fd < 0)
                        continue;

                    setNonBlocking(client_fd);

                    clients.insert(std::make_pair(client_fd, Client(client_fd)));

                    pollfd client_pfd;
                    client_pfd.fd = client_fd;
                    client_pfd.events = POLLIN;
                    client_pfd.revents = 0;
                    fds.push_back(client_pfd);

                    std::cout << "[CONNECT] fd=" << client_fd << std::endl;
                }
                else
                {
                    // ---------- READ ----------
                    std::map<int, Client>::iterator it = clients.find(fds[i].fd);
                    if (it == clients.end())
                        continue;

                    Client &client = it->second;

                    if (!client.readData())
                    {
                        close(fds[i].fd);
                        clients.erase(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--;
                        continue;
                    }

                    if (client.checkRequestComplete())
                    {
                        Request req;
                        std::string raw = client.getRequest();
                        bool parsed = req.parse(raw);

                        if (!parsed && !req.hasError())
                            continue;

                        if (!parsed && req.hasError())
                        {
                            Response res;
                            res.setStatus(400);
                            res.setHeader("Connection", req.getConnectionHeader());
                            res.setBody("Bad Request");
                            client.setResponse(res.build());
                        }
                        else
                        {
                            // TODO: later → use config routing
                            Response res = MethodHandler::handle(req);
                            client.setResponse(res.build());
                        }

                        fds[i].events = POLLOUT;
                    }
                }
            }
            // ---------- WRITE ----------
            else if (fds[i].revents & POLLOUT)
            {
                std::map<int, Client>::iterator it = clients.find(fds[i].fd);
                if (it == clients.end())
                {
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                Client &client = it->second;

                if (client.sendData() < 0)
                {
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                if (client.responseComplete())
                {
                    // TODO: respect Connection header
                    client.reset();
                    fds[i].events = POLLIN;
                }
            }
        }
    }

    return 0;
}