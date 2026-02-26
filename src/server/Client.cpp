#include "Client.hpp"

Client::Client() : _fd(-1), _buffer("")
{
}

Client::Client(int fd) : _fd(fd), _buffer("")
{
}

Client::Client(const Client &src)
{
	*this = src;
}

Client::~Client()
{
}

Client &Client::operator=(const Client &rhs)
{
	if (this != &rhs)
	{
		_fd = rhs._fd;
		_buffer = rhs._buffer;
	}
	return *this;
}

int Client::getFd() const
{
	return _fd;
}

void Client::setFd(int fd)
{
	_fd = fd;
}
