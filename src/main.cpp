#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <ctime>

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

namespace
{
    const int POLL_TIMEOUT_MS = 1000;
    const int CLIENT_IDLE_TIMEOUT_SEC = 60;

    int parseStatusCode(const std::string &status)
    {
        if (status.size() < 3)
            return 400;

        int code = 0;
        for (size_t i = 0; i < 3; i++)
        {
            if (status[i] < '0' || status[i] > '9')
                return 400;
            code = code * 10 + (status[i] - '0');
        }

        return code;
    }

    bool isServerFd(int fd, const std::vector<int> &server_fds)
    {
        for (size_t i = 0; i < server_fds.size(); i++)
        {
            if (server_fds[i] == fd)
                return true;
        }
        return false;
    }

    void removeClientAt(size_t index, std::vector<pollfd> &fds, std::map<int, Client> &clients)
    {
        int fd = fds[index].fd;
        close(fd);
        clients.erase(fd);
        fds.erase(fds.begin() + index);
    }

    bool processClientRequest(Client &client, pollfd &pfd)
    {
        Request req;
        std::string raw = client.getRequest();
        bool parsed = req.parse(raw);

        if (!parsed && !req.hasError())
            return false;

        if (req.hasError())
        {
            Response res;
            int code = parseStatusCode(req.getErrorStatus());
            res.setStatus(code);
            res.setHeader("Connection", "close");
            res.setHeader("Content-Type", "text/plain");
            res.setBody(req.getErrorStatus());
            client.setResponse(res.build());
            client.setCloseAfterResponse(true);
            pfd.events = POLLOUT;
            return true;
        }

        client.setRequestBuffer(raw);

        Response res = MethodHandler::handle(req);
        client.setResponse(res.build());
        client.setCloseAfterResponse(req.shouldClose());
        pfd.events = POLLOUT;
        return true;
    }
}

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
        int poll_ret = poll(&fds[0], fds.size(), POLL_TIMEOUT_MS);
        if (poll_ret < 0)
        {
            perror("poll");
            break;
        }

        time_t now = std::time(NULL);
        for (size_t i = 0; i < fds.size();)
        {
            if (!isServerFd(fds[i].fd, server_fds))
            {
                std::map<int, Client>::iterator it = clients.find(fds[i].fd);
                if (it != clients.end() && it->second.isIdle(now, CLIENT_IDLE_TIMEOUT_SEC))
                {
                    removeClientAt(i, fds, clients);
                    continue;
                }
            }
            i++;
        }

        for (size_t i = 0; i < fds.size(); i++)
        {
            if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                if (isServerFd(fds[i].fd, server_fds))
                {
                    close(fds[i].fd);
                    for (size_t s = 0; s < server_fds.size(); s++)
                    {
                        if (server_fds[s] == fds[i].fd)
                        {
                            server_fds.erase(server_fds.begin() + s);
                            break;
                        }
                    }
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                removeClientAt(i, fds, clients);
                i--;
                continue;
            }

            // ---------- NEW CONNECTION ----------
            if (fds[i].revents & POLLIN)
            {
                // check if server socket
                if (isServerFd(fds[i].fd, server_fds))
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
                        removeClientAt(i, fds, clients);
                        i--;
                        continue;
                    }

                    if (client.hasError())
                    {
                        Response res;
                        res.setStatus(client.getErrorCode());
                        res.setHeader("Connection", "close");
                        res.setHeader("Content-Type", "text/plain");
                        res.setBody("Request too large");
                        client.setResponse(res.build());
                        client.setCloseAfterResponse(true);
                        fds[i].events = POLLOUT;
                        continue;
                    }

                    if (client.checkRequestComplete())
                        processClientRequest(client, fds[i]);
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
                    removeClientAt(i, fds, clients);
                    i--;
                    continue;
                }

                if (client.responseComplete())
                {
                    if (client.shouldCloseAfterResponse())
                    {
                        removeClientAt(i, fds, clients);
                        i--;
                        continue;
                    }

                    std::string remaining = client.getRequest();
                    client.resetForNextRequest(remaining);

                    if (client.hasBufferedData())
                    {
                        if (!processClientRequest(client, fds[i]))
                            fds[i].events = POLLIN;
                    }
                    else
                    {
                        fds[i].events = POLLIN;
                    }
                }
            }
        }
    }

    return 0;
}