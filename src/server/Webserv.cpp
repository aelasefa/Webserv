#include "../../includes/Webserv.hpp"

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
    close(_poll_fds[index].fd);
    _poll_fds.erase(_poll_fds.begin() + index);
}

void Webserv::handleNewConnection()
{
    int client_fd = _socket.acceptClient();
    if (client_fd < 0)
        return;

    addClientToPoll(client_fd);
}

void Webserv::handleClientData(int index)
{
    char buffer[1024];

    int fd = _poll_fds[index].fd;

    int bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0)
    {
        removeClient(index);
        return;
    }

    buffer[bytes] = '\0';

    std::cout << "Request:\n" << buffer << std::endl;

    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello Webserv";

    send(fd, response.c_str(), response.size(), 0);
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