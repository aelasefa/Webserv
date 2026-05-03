#include "../includes/Client.hpp"
#include "../includes/Request.hpp"
#include "../includes/MethodHandler.hpp"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main()
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    {
        std::cerr << "socketpair failed" << std::endl;
        return 1;
    }

    Client client(fds[0]);

    std::string rawRequest =
        "GET /index.html HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    client.appendData(rawRequest);

    Request req;
    std::string buffer = client.getRequest();
    if (!req.parse(buffer) || !req.isDone())
    {
        std::cerr << "Request parsing failed" << std::endl;
        close(fds[0]);
        close(fds[1]);
        return 1;
    }

    Response response = MethodHandler::handle(req);
    client.setResponse(response.build());

    while (!client.responseComplete())
    {
        if (client.sendData() <= 0)
        {
            std::cerr << "sendData failed" << std::endl;
            break;
        }
    }

    shutdown(fds[0], SHUT_WR);

    char out[4096];
    ssize_t n = 0;
    while ((n = read(fds[1], out, sizeof(out))) > 0)
        std::cout.write(out, n);

    close(fds[0]);
    close(fds[1]);

    return 0;
}