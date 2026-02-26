#include "Webserv.hpp"
#include "Logger.hpp"
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
	std::string configFile = "conf/default.conf";

	if (argc > 2)
	{
		std::cerr << "Usage: ./webserv [config_file]" << std::endl;
		return 1;
	}
	if (argc == 2)
		configFile = argv[1];

	try
	{
		Logger::info("Starting webserv...");
		Webserv server(configFile);
		server.run();
	}
	catch (std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}

	return 0;
}
