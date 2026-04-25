#include "../../includes/Webserv.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
    int port;

    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <port>" << std::endl;
        return 1;
    }

    port = std::atoi(argv[1]);

    try
    {
        Webserv server(port);

        server.initServer();
        server.startLoop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}