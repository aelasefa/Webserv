#include "../../includes/NetworkUtils.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <stdexcept>

int NetworkUtils::setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int NetworkUtils::createServerSocket(int port)
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
