#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <string>

class Socket
{
public:
	Socket();
	Socket(int port, const std::string &host);
	Socket(const Socket &src);
	~Socket();
	Socket &operator=(const Socket &rhs);

	void bind();
	void listen();
	int accept();
	void close();

	int getFd() const;

private:
	int _fd;
	int _port;
	std::string _host;
};

#endif
