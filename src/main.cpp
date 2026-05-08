#include <iostream>

#include "../includes/ConfigParser.hpp"
#include "../includes/Webserv.hpp"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <config_file>\n";
        return 1;
    }

    ConfigParser parser;
    Config config = parser.parse(argv[1]);
    try
    {
        Webserv webserv(config.servers);
        webserv.initServers();
        webserv.startLoop();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}