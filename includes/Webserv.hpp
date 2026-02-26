#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <string>
#include <vector>
#include "Server.hpp"
#include "Config.hpp"

class Webserv
{
public:
	Webserv();
	Webserv(const std::string &configFile);
	Webserv(const Webserv &src);
	~Webserv();
	Webserv &operator=(const Webserv &rhs);

	void run();
	void stop();

private:
	Config _config;
	std::vector<Server> _servers;
	bool _running;
};

#endif
