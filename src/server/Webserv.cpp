#include "../../includes/Webserv.hpp"
#include "../../includes/Response.hpp"
#include <fstream>
#include <sstream>

Webserv::Webserv(int port) : _port(port), _socket(port) {}

Webserv::~Webserv() {}

void Webserv::initServer()
{
    if (!_socket.createSocket())
        throw std::runtime_error("Socket creation failed");

    if (!_socket.setSocketOptions())
        throw std::runtime_error("Socket options failed");

    if (!_socket.bindSocket())
        throw std::runtime_error("Bind failed");

    if (!_socket.listenSocket())
        throw std::runtime_error("Listen failed");

    addServerToPoll();
}

void Webserv::addServerToPoll()
{
    pollfd pfd;
    pfd.fd = _socket.getFd();
    pfd.events = POLLIN;
    pfd.revents = 0;

    _poll_fds.push_back(pfd);
}

void Webserv::addClientToPoll(int fd)
{
    _socket.setNonBlocking(fd);

    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    _poll_fds.push_back(pfd);
}

void Webserv::removeClient(int index)
{
    int fd = _poll_fds[index].fd;

    close(fd);
    _clients.erase(fd);
    _poll_fds.erase(_poll_fds.begin() + index);
}

void Webserv::handleNewConnection()
{
    int client_fd = _socket.acceptClient();
    if (client_fd < 0)
        return;

    addClientToPoll(client_fd);
    _clients.insert(std::make_pair(client_fd, Client(client_fd)));
}

void Webserv::handleClientData(int index)
{
    int fd = _poll_fds[index].fd;

    std::map<int, Client>::iterator it = _clients.find(fd);
    if (it == _clients.end())
        return;

    Client &client = it->second;

    char buffer[1024];
    int bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0)
    {
        removeClient(index);
        return;
    }

    buffer[bytes] = '\0';

    client.appendData(std::string(buffer));

    if (!client.checkRequestComplete())
        return;

    std::cout << "FULL REQUEST RECEIVED\n";
    std::cout << client.getRequest() << std::endl;

    Request req = parseRequest(client.getRequest());

    std::string body;
    int status = routeRequest(req, body);

    Response response;
    response.setStatus(status);
    response.setHeader("Content-Type", "text/plain");
    response.setBody(body);

    std::string rawResponse = response.build();
    send(fd, rawResponse.c_str(), rawResponse.size(), 0);

    client.reset();
}

int Webserv::routeRequest(const Request &req, std::string &body)
{
    if (req.method == "GET")
        return handleGET(req.path, body);

    // else if (req.method == "POST")
    //     return handlePOST(req.path, req.body);

    // else if (req.method == "DELETE")
    //     return handleDELETE(req.path);

    body = "Method Not Allowed";
    return 405;
}

int Webserv::handleGET(const std::string &path, std::string &body)
{
    std::string filePath = path;

    if (filePath == "/")
        filePath = "/index.html";

    if (filePath.find("..") != std::string::npos)
    {
        body = "Forbidden";
        return 403;
    }

    std::string fullPath = "./www" + filePath;
    std::ifstream file(fullPath.c_str());
    if (!file.is_open())
    {
        body = "Not Found";
        return 404;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    body = buffer.str();
    return 200;
}

Request Webserv::parseRequest(const std::string &raw)
{
    Request req;

    size_t line_end = raw.find("\r\n");
    std::string line = raw.substr(0, line_end);

    size_t s1 = line.find(' ');
    size_t s2 = line.find(' ', s1 + 1);

    req.method = line.substr(0, s1);
    req.path = line.substr(s1 + 1, s2 - s1 - 1);
    req.version = line.substr(s2 + 1);

    return req;
}

void Webserv::startLoop()
{
    while (true)
    {
        int ret = poll(&_poll_fds[0], _poll_fds.size(), -1);
        if (ret < 0)
        {
            std::cerr << "Poll error\n";
            continue;
        }
        for (size_t i = 0; i < _poll_fds.size(); i++)
        {
            if (_poll_fds[i].revents & POLLIN)
            {
                if (_poll_fds[i].fd == _socket.getFd())
                {
                    handleNewConnection();
                }
                else
                {
                    handleClientData(i);
                }
            }
        }
    }
}
