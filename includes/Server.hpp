#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>

class Server
{
public:
	Server();
	Server(const Server &src);
	~Server();
	Server &operator=(const Server &rhs);

	void start();
	void stop();

private:
	int _port;
	std::string _host;
};

#endif
