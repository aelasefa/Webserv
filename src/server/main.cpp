#include "../../includes/Webserv.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <port>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);

    if (port <= 0 || port > 65535)
    {
        std::cerr << "Invalid port" << std::endl;
        return 1;
    }

    try
    {
        Webserv server(port);

        std::cout << "Starting server on port " << port << "...\n";

        server.initServer();

        std::cout << "Server is running (poll loop started)...\n";

        server.startLoop();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}