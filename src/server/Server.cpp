#include "Server.hpp"

Server::Server() : _port(8080), _host("0.0.0.0")
{
}

Server::Server(const Server &src)
{
	*this = src;
}

Server::~Server()
{
}

Server &Server::operator=(const Server &rhs)
{
	if (this != &rhs)
	{
		_port = rhs._port;
		_host = rhs._host;
	}
	return *this;
}

void Server::start()
{
}

void Server::stop()
{
}
