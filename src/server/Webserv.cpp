#include "../../includes/Webserv.hpp"
#include "../../includes/Router.hpp"
#include "../../includes/Request.hpp"
#include "../../includes/Response.hpp"
#include "../../includes/StringUtils.hpp"
#include "../../includes/NetworkUtils.hpp"
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
#include <cerrno>

static const int POLL_TIMEOUT_MS        = 1000;
static const int CLIENT_IDLE_TIMEOUT_SEC = 60;

static void parseHostHeader(const std::string &hostHeader, std::string &host, int &port)
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

static const Server &selectServerByHost(const std::vector<Server> &servers,
                                        const Server &defaultServer,
                                        const std::string &hostHeader)
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

static int parseStatusCode(const std::string &status)
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

Webserv::Webserv(const std::vector<Server> &servers) : _servers(servers) {}

Webserv::~Webserv()
{
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->first);
    }
    _clients.clear();

    for (size_t i = 0; i < _server_fds.size(); ++i)
    {
        close(_server_fds[i]);
    }
    _server_fds.clear();
}

static std::string generateSessionId()
{
    unsigned char buf[32];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0)
    {
        ssize_t got = read(fd, buf, sizeof(buf));
        close(fd);
        if (got == static_cast<ssize_t>(sizeof(buf)))
        {
            static const char hex[] = "0123456789abcdef";
            std::string id;
            id.reserve(64);
            for (int i = 0; i < 32; ++i)
            {
                id += hex[(buf[i] >> 4) & 0xF];
                id += hex[buf[i] & 0xF];
            }
            return id;
        }
    }
    static size_t ctr = 0;
    std::ostringstream oss;
    oss << "fb_" << (size_t)std::time(NULL) << "_" << getpid() << "_" << (ctr++);
    return oss.str();
}

void Webserv::initServers()
{
    if (_servers.empty())
        throw std::runtime_error("no server defined in config");

    for (size_t i = 0; i < _servers.size(); i++)
    {
        int server_fd = NetworkUtils::createServerSocket(_servers[i].listen);
        if (server_fd == -1)
            continue;
        addServerToPoll(server_fd);
        _server_fds.push_back(server_fd);
    }
}

void Webserv::addServerToPoll(int server_fd)
{
    pollfd pfd;
    pfd.fd      = server_fd;
    pfd.events  = POLLIN;
    pfd.revents = 0;
    _poll_fds.push_back(pfd);
}

void Webserv::addClientToPoll(int fd)
{
    NetworkUtils::setNonBlocking(fd);

    pollfd pfd;
    pfd.fd      = fd;
    pfd.events  = POLLIN;
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
        return;
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
    res.setConnection("close");

    std::map<int, std::string>::const_iterator it = server.error_pages.find(code);

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
        res.setBody("Error " + StringUtils::toString(code));
    }

    return res;
}

bool Webserv::processClientRequest(Client &client, Request &req, pollfd &pfd)
{
    const size_t index        = client.getServerIndex();
    const Server &defaultServer = (index < _servers.size()) ? _servers[index] : _servers[0];
    std::string hostHeader    = req.getHeader("Host");
    const Server &server      = selectServerByHost(_servers, defaultServer, hostHeader);
    std::string cookieHeader  = req.getHeader("Cookie");

    std::string sessionId = Request::getCookieValue(cookieHeader, "session_id");
    std::string theme     = Request::getCookieValue(cookieHeader, "theme");

    bool newSession        = false;
    bool themeCookieNeeded = false;

    if (sessionId.empty())
    {
        sessionId = generateSessionId();
        _sessionManager.create(sessionId, "light");
        theme             = "light";
        newSession        = true;
        themeCookieNeeded = true;
    }
    else
    {
        if (!_sessionManager.exists(sessionId))
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

    Response res = Router::routeRequest(server, req);


    int code = res.getStatusCode();

    if (code == 0)
    {
        std::string scriptPath;
        std::string interpreter;
        const std::vector< std::pair<std::string, std::string> > &hdrs = res.getHeaderList();
        for (size_t h = 0; h < hdrs.size(); ++h)
        {
            if (hdrs[h].first == "X-CGI-Script")
                scriptPath = hdrs[h].second;
            if (hdrs[h].first == "X-CGI-Interpreter")
                interpreter = hdrs[h].second;
        }

        try
        {
            client.startCgi(scriptPath, interpreter, req, req.shouldClose(), server);
            pfd.events = POLLIN;
            return true;
        }
        catch (const std::exception &)
        {
            Response errRes = buildErrorResponse(server, 502);
            client.setResponse(errRes.build());
            client.setCloseAfterResponse(true);
            pfd.events = POLLIN | POLLOUT;
            return true;
        }
    }

    if (code >= 400)
    {
        Response errorPage = buildErrorResponse(server, code);
        if (!errorPage.getBody().empty())
            res = errorPage;
    }

    if (newSession)
        res.addCookie("session_id=" + sessionId + "; Path=/; HttpOnly; SameSite=Lax");
    if (themeCookieNeeded)
        res.addCookie("theme=" + theme + "; Path=/; Max-Age=2592000; SameSite=Lax");

    bool responseWantsClose = false;
    const std::vector< std::pair<std::string, std::string> > &hdrs = res.getHeaderList();
    for (size_t h = 0; h < hdrs.size(); ++h)
    {
        std::string keyLower = hdrs[h].first;
        for (size_t i = 0; i < keyLower.size(); ++i)
            keyLower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(keyLower[i])));
        if (keyLower == "connection")
        {
            std::string valLower = hdrs[h].second;
            for (size_t i = 0; i < valLower.size(); ++i)
                valLower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(valLower[i])));
            if (valLower.find("close") != std::string::npos)
                responseWantsClose = true;
        }
    }
    if (res.getConnection() == "close")
        responseWantsClose = true;

    bool finalShouldClose = req.shouldClose() || responseWantsClose;
    res.setConnection(finalShouldClose ? "close" : "keep-alive");

    if (res.isFileBody())
    {
        bool opened = client.setResponseHeaderAndFile(
            res.buildHeaders(), res.getFilePath(), res.getFileSize());
        client.setCloseAfterResponse(opened ? finalShouldClose : true);
    }
    else
    {
        client.setResponse(res.build());
        client.setCloseAfterResponse(finalShouldClose);
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

    if (client.hasResponse())
        return;

    const size_t serverIndex = client.getServerIndex();
    const Server &server =
        (serverIndex < _servers.size()) ? _servers[serverIndex] : _servers[0];

    if (client.hasError())
    {
        int code = client.getErrorCode();
        client.setResponse(buildErrorResponse(server, code).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    if (server.client_max_body_size > 0 &&
        client.getRequestBuffer().size() > (size_t)server.client_max_body_size)
    {
        client.setResponse(buildErrorResponse(server, 413).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    Request &req     = client.getParser();
    std::string &buffer = client.getRequestBuffer();

    bool parsed = req.parse(buffer);

    if (!parsed && !req.hasError())
        return;

    if (req.hasError())
    {
        int code = parseStatusCode(req.getErrorStatus());
        client.setResponse(buildErrorResponse(server, code).build());
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

    Request &req        = client.getParser();
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
            (serverIndex < _servers.size()) ? _servers[serverIndex] : _servers[0];

        int code = parseStatusCode(req.getErrorStatus());
        client.setResponse(buildErrorResponse(server, code).build());
        client.setCloseAfterResponse(true);
        _poll_fds[index].events = POLLIN | POLLOUT;
        return;
    }

    _poll_fds[index].events = POLLIN;
}

void Webserv::setNonBlocking(int fd)
{
    NetworkUtils::setNonBlocking(fd);
}

void Webserv::pollRunningCgis()
{
    for (std::map<int, Client>::iterator it = _clients.begin();
         it != _clients.end(); ++it)
    {
        Client &client = it->second;
        if (!client.isCgiRunning())
            continue;

        if (client.updateCgi())
        {
            CGIState &state = client.getCgiState();
            Response res;

            if (client.hasError())
            {
                const size_t idx    = client.getServerIndex();
                const Server &srv   = (idx < _servers.size()) ? _servers[idx] : _servers[0];
                res = buildErrorResponse(srv, client.getErrorCode());
                client.setCloseAfterResponse(true);
            }
            else if (state.result.empty())
            {
                const size_t idx    = client.getServerIndex();
                const Server &srv   = (idx < _servers.size()) ? _servers[idx] : _servers[0];
                res = buildErrorResponse(srv, 504);
                client.setCloseAfterResponse(true);
            }
            else
            {
                res = CGIHandler::buildResponse(state.result);
                bool responseWantsClose = false;
                const std::vector< std::pair<std::string, std::string> > &hdrs = res.getHeaderList();
                for (size_t h = 0; h < hdrs.size(); ++h)
                {
                    std::string keyLower = hdrs[h].first;
                    for (size_t i = 0; i < keyLower.size(); ++i)
                        keyLower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(keyLower[i])));
                    if (keyLower == "connection")
                    {
                        std::string valLower = hdrs[h].second;
                        for (size_t i = 0; i < valLower.size(); ++i)
                            valLower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(valLower[i])));
                        if (valLower.find("close") != std::string::npos)
                            responseWantsClose = true;
                    }
                }
                if (res.getConnection() == "close")
                    responseWantsClose = true;

                bool finalShouldClose = client.getCgiShouldClose() || responseWantsClose;
                res.setConnection(finalShouldClose ? "close" : "keep-alive");
                client.setCloseAfterResponse(finalShouldClose);
            }

            client.setResponse(res.build());
            client.clearCgi();

            for (size_t p = 0; p < _poll_fds.size(); ++p)
            {
                if (_poll_fds[p].fd == it->first)
                {
                    _poll_fds[p].events = POLLIN | POLLOUT;
                    break;
                }
            }
        }
    }
}

void Webserv::startLoop()
{
    if (_poll_fds.empty())
        throw std::runtime_error("no server socket could be bound");

    while (g_running)
    {
        int poll_ret = poll(&_poll_fds[0], _poll_fds.size(), POLL_TIMEOUT_MS);
        if (poll_ret < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                if (!g_running)
                    break;
                continue;
            }
            perror("poll");
            sleep(1);
            continue;
        }
        time_t now = std::time(NULL);

        pollRunningCgis();

        for (size_t i = 0; i < _poll_fds.size();)
        {
            if (!isServerFd(_poll_fds[i].fd))
            {
                std::map<int, Client>::iterator it = _clients.find(_poll_fds[i].fd);
                if (it != _clients.end() &&
                    it->second.isIdle(now, CLIENT_IDLE_TIMEOUT_SEC))
                {
                    removeClientAt(i);
                    continue;
                }
            }
            i++;
        }

        for (size_t i = 0; i < _poll_fds.size();)
        {
            if (_poll_fds[i].revents & (POLLERR | POLLNVAL))
            {
                if (!isServerFd(_poll_fds[i].fd))
                    removeClientAt(i);
                else
                    i++;
                continue;
            }
            if (_poll_fds[i].revents & POLLIN)
            {
                if (isServerFd(_poll_fds[i].fd))
                    handleNewConnection(_poll_fds[i].fd);
                else
                {
                    size_t old = _poll_fds.size();
                    handleClientRead(i);
                    if (_poll_fds.size() < old)
                        continue;
                }
            }
            if (i < _poll_fds.size() && (_poll_fds[i].revents & POLLOUT))
                handleClientWrite(i);
            if (i < _poll_fds.size() &&
                (_poll_fds[i].revents & POLLHUP) &&
                !isServerFd(_poll_fds[i].fd))
            {
                removeClientAt(i);
                continue;
            }
            i++;
        }
    }
}