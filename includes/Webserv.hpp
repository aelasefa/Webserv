#ifndef WEBSERV_HPP
#define WEBSERV_HPP

#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>

class Webserv
{
private:
    int _port;
    int _server_fd;
    sockaddr_in _address;

public:
    Webserv(int port);
    ~Webserv();
    void initServer();
    void bindServer();
    void startListening();

    void startLoop();
    int acceptClient();

    int getServerFD() const;
};
#endif