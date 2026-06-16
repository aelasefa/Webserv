#include "../../includes/Webserv.hpp"
#include "../../includes/Router.hpp"
#include "../../includes/Request.hpp"
#include "../../includes/Response.hpp"
#include "../../includes/Utils.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

namespace
{
    const int POLL_TIMEOUT_MS = 1000;
    const int CLIENT_IDLE_TIMEOUT_SEC = 60;

    void parseHostHeader(const std::string &hostHeader, std::string &host, int &port)
    {
        host.clear();
        port = 0;

        if (hostHeader.empty())
            return;

        std::string value = hostHeader;
        size_t colon = value.find(':');
        if (colon == std::string::npos)
        {
            host = value;
            return;
        }

        host = value.substr(0, colon);
        std::string portStr = value.substr(colon + 1);
        if (!portStr.empty())
            port = std::atoi(portStr.c_str());
    }

    const Server &selectServerByHost(const std::vector<Server> &servers, const Server &defaultServer, const std::string &hostHeader)
    {
        std::string host;
        int port = 0;
        parseHostHeader(hostHeader, host, port);

        int effectivePort = (port > 0) ? port : defaultServer.listen;

        for (size_t i = 0; i < servers.size(); i++)
        {
            if (servers[i].listen != effectivePort)
                continue;

            if (!servers[i].host.empty() && servers[i].host == host)
                return servers[i];

            if (!servers[i].server_name.empty() && servers[i].server_name == host)
                return servers[i];
        }

        for (size_t i = 0; i < servers.size(); i++)
        {
            if (servers[i].listen == effectivePort)
                return servers[i];
        }

        return defaultServer;
    }

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

    int setNonBlocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
            return -1;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    int createServerSocket(int port)
    {
        std::cout << "[SERVER] Creating socket..." << std::endl;
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
            throw std::runtime_error("socket failed");
        std::cout << "[SERVER] Socket created fd=" << server_fd << std::endl;
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        std::cout << "[SERVER] Binding to port " << port << std::endl;
        if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
            throw std::runtime_error("bind failed");
        std::cout << "[SERVER] Bind successful" << std::endl;
        std::cout << "[SERVER] Listening on port "
          << port << std::endl;
        if (listen(server_fd, SOMAXCONN) < 0)
            throw std::runtime_error("listen failed");

        setNonBlocking(server_fd);

        return server_fd;
    }
}

Webserv::Webserv(const std::vector<Server> &servers) : _servers(servers) {}

Webserv::~Webserv() {}

void Webserv::initServers()
{
    if (_servers.empty())
        throw std::runtime_error("no server defined in config");

    for (size_t i = 0; i < _servers.size(); i++)
    {
        int server_fd = createServerSocket(_servers[i].listen);
        addServerToPoll(server_fd);
        _server_fds.push_back(server_fd);
    }
}

void Webserv::addServerToPoll(int server_fd)
{
    pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    _poll_fds.push_back(pfd);
}

void Webserv::addClientToPoll(int fd)
{
    setNonBlocking(fd);

    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    _poll_fds.push_back(pfd);
}

bool Webserv::isServerFd(int fd) const
{
    for (size_t i = 0; i < _server_fds.size(); i++)
    {
        if (_server_fds[i] == fd)
            return true;
    }
    return false;
}

void Webserv::removeClientAt(size_t index)
{
    int fd = _poll_fds[index].fd;
    close(fd);
    _clients.erase(fd);
    _poll_fds.erase(_poll_fds.begin() + index);
}

void Webserv::handleNewConnection(int server_fd)
{
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0)
    {
        perror("accept");
        return ;
    }

    addClientToPoll(client_fd);
    size_t serverIndex = 0;
    for (size_t i = 0; i < _server_fds.size(); i++)
    {
        if (_server_fds[i] == server_fd)
        {
            serverIndex = i;
            break;
        }
    }
    _clients.insert(std::make_pair(client_fd, Client(client_fd, serverIndex)));
}

Response Webserv::buildErrorResponse(const Server &server, int code)
{
    Response res;
    res.setStatus(code);
    res.setHeader("Connection", "close");

    std::map<int, std::string>::const_iterator it =
        server.error_pages.find(code);

    std::string path;
    if (it != server.error_pages.end())
    {
        path = server.root;
        if (!path.empty() && path[path.size() - 1] != '/')
            path += "/";
        path += it->second;
    }

    std::ifstream file(path.c_str());
    if (file)
    {
        std::stringstream ss;
        ss << file.rdbuf();
        res.setHeader("Content-Type", "text/html");
        res.setBody(ss.str());
    }
    else
    {
        res.setHeader("Content-Type", "text/plain");
        res.setBody("Error " + Utils::toString(code));
    }

    return res;
}

bool Webserv::processClientRequest(Client &client, Request &req, pollfd &pfd)
{
    const size_t index = client.getServerIndex();
    const Server &defaultServer = (index < _servers.size()) ? _servers[index] : _servers[0];
    std::string hostHeader = req.getHeader("Host");
    const Server &server = selectServerByHost(_servers, defaultServer, hostHeader);
    std::string cookieHeader = req.getHeader("Cookie");
    

    std::string sessionId = Request::getCookieValue(cookieHeader, "session_id");
    std::string theme      = Request::getCookieValue(cookieHeader, "theme");
    

    bool newSession = false;
    bool themeCookieNeeded = false;

    if (sessionId.empty())
    {
        sessionId = "id_" + Utils::toString(std::time(NULL));
        _sessionManager.create(sessionId, "light");
        theme = "light";
        newSession = true;
        themeCookieNeeded = true;
    }
    else
    {
        if (!_sessionManager.exists(sessionId))
        {
            _sessionManager.create(sessionId, "light");
            if (theme.empty())
            {
                theme = "light";
                themeCookieNeeded = true;
            }
            newSession = true;
        }
        else
        {
            std::string stored = _sessionManager.getTheme(sessionId);
            if (theme.empty())
            {
                if (stored.empty()) stored = "light";
                theme = stored;
                _sessionManager.setTheme(sessionId, theme);
                themeCookieNeeded = true;
            }
            else
            {
                if (theme != stored)
                    _sessionManager.setTheme(sessionId, theme);
            }
        }
    }

    if (server.client_max_body_size > 0 && req.getBody().size() > static_cast<size_t>(server.client_max_body_size))
    {
        Response res;
        res.setStatus(413);
        res.setHeader("Connection", "close");
        res.setHeader("Content-Type", "text/plain");
        res.setBody("413 Payload Too Large");
        client.setResponse(res.build());
        client.setCloseAfterResponse(true);
        pfd.events = POLLIN | POLLOUT;
        return true;
    }
    Response res = Router::routeRequest(server, req);
    int code = res.getStatusCode();
    if (code >= 400)
    {
        res = buildErrorResponse(server, code);
    }
    if (newSession) 
    {
        std::string val = std::string("session_id=") + sessionId + "; Path=/";
        res.setHeader("Set-Cookie", val);
    }
    if (themeCookieNeeded)
    {
        std::string val = std::string("theme=") + theme + "; Path=/";
        res.setHeader("Set-Cookie", val);
    }
    client.setResponse(res.build());
    client.setCloseAfterResponse(req.shouldClose());
    pfd.events = POLLIN | POLLOUT;
    return true;
}

void Webserv::handleClientRead(size_t index)
{
    int fd = _poll_fds[index].fd;

    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client &client = it->second;

    if (!client.readData())
    {
        removeClientAt(index);
        return;
    }

    const size_t serverIndex = client.getServerIndex();
    const Server &server =
        (serverIndex < _servers.size()) ? _servers[serverIndex] : _servers[0];

    if (client.hasError())
    {
        int code = client.getErrorCode();

        client.setResponse(
            buildErrorResponse(server, code).build()
        );

        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    if (server.client_max_body_size > 0 &&
        client.getRequestBuffer().size() > (size_t)server.client_max_body_size)
    {
        client.setResponse(
            buildErrorResponse(server, 413).build()
        );

        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    Request &req = client.getParser();
    std::string &buffer = client.getRequestBuffer();

    bool parsed = req.parse(buffer);

    if (!parsed && !req.hasError())
        return;

    if (req.hasError())
    {
        int code = parseStatusCode(req.getErrorStatus());

        client.setResponse(
            buildErrorResponse(server, code).build()
        );

        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    if (req.isDone())
        processClientRequest(client, req, _poll_fds[index]);
}

void Webserv::handleClientWrite(size_t index)
{
    int fd = _poll_fds[index].fd;

    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it == _clients.end())
    {
        _poll_fds.erase(_poll_fds.begin() + index);
        return;
    }

    Client &client = it->second;

    if (!client.hasResponse())
        return;

    if (client.sendData() < 0)
    {
        removeClientAt(index);
        return;
    }

    if (!client.responseComplete())
        return;

    if (client.shouldCloseAfterResponse())
    {
        removeClientAt(index);
        return;
    }

    std::string remaining = client.getRequest();
    client.resetForNextRequest(remaining);

    if (!client.hasBufferedData())
    {
        _poll_fds[index].events = POLLIN;
        return;
    }

    Request &req = client.getParser();
    std::string &buffer = client.getRequestBuffer();

    bool parsed = req.parse(buffer);

    if (parsed)
    {
        processClientRequest(client, req, _poll_fds[index]);
        return;
    }

    if (req.hasError())
    {
        const size_t serverIndex = client.getServerIndex();
        const Server &server =
            (serverIndex < _servers.size())
                ? _servers[serverIndex]
                : _servers[0];

        int code = parseStatusCode(req.getErrorStatus());

        client.setResponse(
            buildErrorResponse(server, code).build()
        );

        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    _poll_fds[index].events = POLLIN;
}

void Webserv::startLoop()
{
    while (true)
    {
        int poll_ret = poll(&_poll_fds[0], _poll_fds.size(), POLL_TIMEOUT_MS);
        if (poll_ret < 0)
        {
            perror("poll");
            break;
        }

        time_t now = std::time(NULL);
        for (size_t i = 0; i < _poll_fds.size();)
        {
            if (!isServerFd(_poll_fds[i].fd))
            {
                std::map<int, Client>::iterator it = _clients.find(_poll_fds[i].fd);
                if (it != _clients.end() && it->second.isIdle(now, CLIENT_IDLE_TIMEOUT_SEC))
                {
                    removeClientAt(i);
                    continue;
                }
            }
            i++;
        }
        size_t size = _poll_fds.size();
        for (size_t i = 0; i < size; i++)
        {
            if (_poll_fds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                if (isServerFd(_poll_fds[i].fd))
                {

                    std::cerr << "[WARNING] Server socket event error ignored fd="
                              << _poll_fds[i].fd << std::endl;
                    continue;
                }

                removeClientAt(i);
                i--;
                continue;
            }

            if (_poll_fds[i].revents & POLLIN)
            {
                if (isServerFd(_poll_fds[i].fd))
                {
                    handleNewConnection(_poll_fds[i].fd);
                }
                else
                {
                    int fd_before = _poll_fds[i].fd;

                    handleClientRead(i);

                    if (i >= _poll_fds.size())
                        continue;

                    if (_poll_fds[i].fd != fd_before)
                        continue;
                }
            }
            if (_poll_fds[i].revents & POLLOUT)
            {
                handleClientWrite(i);
            }
        }
    }
}