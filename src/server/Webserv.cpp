#include "../../includes/Webserv.hpp"
#include <cerrno>

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

    if (bytes == 0)
    {
        std::cout << "Client disconnected fd=" << fd << std::endl;
        removeClient(index);
        return;
    }
    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        std::cerr << "recv error on fd=" << fd << std::endl;
        removeClient(index);
        return;
    }

    buffer[bytes] = '\0';

    client.appendData(std::string(buffer));

    if (!client.checkRequestComplete())
        return;

    std::cout << "FULL REQUEST \n";
    std::cout << client.getRequest() << std::endl;

    //TEMP RESPONSE (until HTTP methods implemented)
    std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 13\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello Webserv";

    send(fd, response.c_str(), response.size(), 0);

    client.reset();
}

void Webserv::startLoop()
{
    while (true)
    {
        int ret = poll(&_poll_fds[0], _poll_fds.size(), -1);

        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "Poll fatal error\n";
            break;
        }

        for (size_t i = 0; i < _poll_fds.size();)
        {
            short revents = _poll_fds[i].revents;

            if (revents == 0)
            {
                i++;
                continue;
            }

            int fd = _poll_fds[i].fd;

            if (revents & (POLLHUP | POLLERR | POLLNVAL))
            {
                std::cout << "Client error/disconnect fd=" << fd << std::endl;
                removeClient(i);
                continue;
            }

            if (fd == _socket.getFd())
            {
                if (revents & POLLIN)
                    handleNewConnection();
            }
            else
            {
                if (revents & POLLIN)
                    handleClientData(i);

                // if (revents & POLLOUT)
                //     handleSend(i);
            }

            i++;
        }
    }
}