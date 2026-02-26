#include "Socket.hpp"

Socket::Socket() : _fd(-1), _port(0), _host("")
{
}

Socket::Socket(int port, const std::string &host) : _fd(-1), _port(port), _host(host)
{
}

Socket::Socket(const Socket &src)
{
	*this = src;
}

Socket::~Socket()
{
}

Socket &Socket::operator=(const Socket &rhs)
{
	if (this != &rhs)
	{
		_fd = rhs._fd;
		_port = rhs._port;
		_host = rhs._host;
	}
	return *this;
}

void Socket::bind()
{
}

void Socket::listen()
{
}

int Socket::accept()
{
	return -1;
}

void Socket::close()
{
}

int Socket::getFd() const
{
	return _fd;
}
