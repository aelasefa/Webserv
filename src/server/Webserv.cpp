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
#include <signal.h>
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
        {
            std::cerr << "[ERROR] bind failed on port "
                      << port
                      << ": "
                      << strerror(errno)
                      << std::endl;

            close(server_fd);
            return -1;
        }
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
        if (server_fd == -1)
            continue;
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
    const size_t  si            = client.getServerIndex();
    const Server &defaultServer = (si < _servers.size()) ? _servers[si] : _servers[0];
    const Server server        = selectServerByHost(
        _servers, defaultServer, req.getHeader("Host")
    );

    std::string cookieHeader = req.getHeader("Cookie");
    std::string sessionId    = Request::getCookieValue(cookieHeader, "session_id");
    std::string theme        = Request::getCookieValue(cookieHeader, "theme");

    bool newSession       = false;
    bool themeCookieNeeded = false;

    if (sessionId.empty())
    {
        static size_t _sessionCounter = 0;
        sessionId = "id_"
            + Utils::toString((size_t)std::time(NULL))
            + "_" + Utils::toString(client.getFd())
            + "_" + Utils::toString(_sessionCounter++);

        _sessionManager.create(sessionId, "light");
        theme             = "light";
        newSession        = true;
        themeCookieNeeded = true;
    }
    else if (!_sessionManager.exists(sessionId))
    {
        _sessionManager.create(sessionId, "light");
        if (theme.empty())
        {
            theme             = "light";
            themeCookieNeeded = true;
        }
        newSession = true;
    }
    else
    {
        std::string stored = _sessionManager.getTheme(sessionId);
        if (stored.empty()) stored = "light";

        if (theme.empty())
        {
            theme = stored;
            _sessionManager.setTheme(sessionId, theme);
            themeCookieNeeded = true;
        }
        else if (theme != stored)
        {
            _sessionManager.setTheme(sessionId, theme);
        }
    }

    if (server.client_max_body_size > 0 &&
        req.getBody().size() > static_cast<size_t>(server.client_max_body_size))
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

    Response res  = Router::routeRequest(server, req);
    int      code = res.getStatusCode();

    if (req.getMethod() == "HEAD")
        res.setBody("");

    if (code >= 400 && !server.error_pages.empty())
    {
        Response errorPage = buildErrorResponse(server, code);
        if (!errorPage.getBody().empty())
            res.setBody(errorPage.getBody());
    }

    if (newSession)
        res.setHeader("Set-Cookie", "session_id=" + sessionId + "; Path=/");
    if (themeCookieNeeded)
        res.setHeader("Set-Cookie", "theme=" + theme + "; Path=/");

    if (res.isFileBody())
    {
        bool opened = client.setResponseHeaderAndFile(
            res.buildHeaders(), res.getFilePath(), res.getFileSize()
        );
        client.setCloseAfterResponse(opened ? req.shouldClose() : true);
    }
    else
    {
        client.setResponse(res.build());
        client.setCloseAfterResponse(req.shouldClose());
    }

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

    const size_t  si     = client.getServerIndex();
    const Server &server = (si < _servers.size()) ? _servers[si] : _servers[0];

    if (client.hasError())
    {
        int code = client.getErrorCode();
        client.setResponse(buildErrorResponse(server, code).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    Request     &req    = client.getParser();
    std::string &buffer = client.getRequestBuffer();

    bool parsed = req.parse(buffer);

    if (!parsed && !req.hasError())
    {
        if (client.isPeerClosed())
        {
            removeClientAt(index);
            return;
        }
        return; 
    }

    if (req.hasError())
    {
        int code = parseStatusCode(req.getErrorStatus());
        client.setResponse(buildErrorResponse(server, code).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    if (req.isDone())
    {
        if (client.isPeerClosed())
            client.setCloseAfterResponse(true);

        processClientRequest(client, req, _poll_fds[index]);
    }
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
    {
        _poll_fds[index].events = POLLIN;
        return;
    }

    ssize_t sent = client.sendData();
    if (sent < 0)
    {
        removeClientAt(index);
        return;
    }

    if (client.hasResponse())
    {
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }


    if (client.shouldCloseAfterResponse())
    {
        removeClientAt(index);
        return;
    }

    _poll_fds[index].events = POLLIN;

    std::string remaining = client.getRequest();
    client.resetForNextRequest(remaining);

    if (!client.hasBufferedData())
        return;

    Request     &req    = client.getParser();
    std::string &buffer = client.getRequestBuffer();

    bool parsed = req.parse(buffer);

    if (parsed && req.isDone())
    {
        processClientRequest(client, req, _poll_fds[index]);
        return;
    }

    if (req.hasError())
    {
        const size_t  si     = client.getServerIndex();
        const Server &server = (si < _servers.size()) ? _servers[si] : _servers[0];

        int code = parseStatusCode(req.getErrorStatus());
        client.setResponse(buildErrorResponse(server, code).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    _poll_fds[index].events = POLLIN;
}

void Webserv::setNonBlocking(int fd) {

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl non-blocking failed\n";
    }
}

void Webserv::checkTimeouts()
{
    static const time_t IDLE_TIMEOUT_SEC = 60;

    time_t now = std::time(NULL);

    for (size_t i = 0; i < _poll_fds.size(); )
    {
        int fd = _poll_fds[i].fd;

        if (isServerFd(fd)) { i++; continue; }

        std::map<int, Client>::iterator it = _clients.find(fd);
        if (it == _clients.end())
        {
            _poll_fds.erase(_poll_fds.begin() + i);
            continue;
        }

        Client &client = it->second;

        if (client.hasResponse())
        {
            i++; continue;
        }

        time_t lastActivity = client.getLastActivityTime();
        if (now - lastActivity > IDLE_TIMEOUT_SEC)
        {
            const size_t  si     = client.getServerIndex();
            const Server &server = (si < _servers.size())
                ? _servers[si] : _servers[0];

            Response res = buildErrorResponse(server, 408);
            res.setHeader("Connection", "close");
            client.setResponse(res.build());
            client.setCloseAfterResponse(true);
            _poll_fds[i].events = POLLOUT;

            i++;
            continue;
        }

        i++;
    }
}

void Webserv::startLoop()
{
      signal(SIGPIPE, SIG_IGN);

    while (true)
    {
        int ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);

        if (ret < 0)
        {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }

        checkTimeouts();  

        if (ret == 0)
            continue;

        for (size_t i = 0; i < _poll_fds.size(); )
        {
            if (_poll_fds[i].revents == 0) { i++; continue; }

            int fd = _poll_fds[i].fd;

            if (_poll_fds[i].revents & (POLLERR | POLLNVAL))
            {
                if (!isServerFd(fd))
                    removeClientAt(i);   
                else
                    i++;
                continue;
            }

            if (_poll_fds[i].revents & POLLIN)
            {
                if (isServerFd(fd))
                {
                    handleNewConnection(fd);
                }
                else
                {
                    size_t oldSize = _poll_fds.size();
                    handleClientRead(i);
                    if (_poll_fds.size() < oldSize || _poll_fds[i].fd != fd)
                        continue;
                }
            }

            if (i < _poll_fds.size()
                && _poll_fds[i].fd == fd
                && !isServerFd(fd)
                && (_poll_fds[i].revents & POLLOUT))
            {
                size_t oldSize = _poll_fds.size();
                handleClientWrite(i);
                if (_poll_fds.size() < oldSize ||
                    (i < _poll_fds.size() && _poll_fds[i].fd != fd))
                    continue;   
            }

            if (i < _poll_fds.size()
                && _poll_fds[i].fd == fd
                && (_poll_fds[i].revents & POLLHUP)
                && !isServerFd(fd))
            {
                removeClientAt(i);
                continue;
            }

            i++;
        }
    }
}