#include "Webserv.hpp"

Webserv::Webserv() : _running(false)
{
}

Webserv::Webserv(const std::string &configFile) : _config(configFile), _running(false)
{
}

Webserv::Webserv(const Webserv &src)
{
	*this = src;
}

Webserv::~Webserv()
{
}

Webserv &Webserv::operator=(const Webserv &rhs)
{
	if (this != &rhs)
	{
		_config = rhs._config;
		_servers = rhs._servers;
		_running = rhs._running;
	}
	return *this;
}

void Webserv::run()
{
	_running = true;
}

void Webserv::stop()
{
	_running = false;
}
