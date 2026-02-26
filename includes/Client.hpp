#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client
{
public:
	Client();
	Client(int fd);
	Client(const Client &src);
	~Client();
	Client &operator=(const Client &rhs);

	int getFd() const;
	void setFd(int fd);

private:
	int _fd;
	std::string _buffer;
};

#endif
