#include <iostream>
#include <signal.h>
#include "../includes/ConfigParser.hpp"
#include "../includes/Webserv.hpp"

volatile sig_atomic_t g_running = 1;

void signalHandler(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: ./webserv <config_file>\n";
        return 1;
    }

    std::string configPath = argv[1];

    ConfigParser parser;
    Config config = parser.parse(configPath);

    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        std::cerr << "Error: cannot setup SIGINT handler\n";
        return 1;
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        std::cerr << "Error: cannot setup SIGTERM handler\n";
        return 1;
    }

    try
    {
        Webserv webserv(config.servers);
        signal(SIGPIPE, SIG_IGN);
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