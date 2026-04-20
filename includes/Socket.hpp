#ifndef SOCKET_HPP
#define  SOCKET_HPP

#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
class Socket {
    private:
        int _fd;
        sockaddr_in servaddr;
    public:
        bool CreatSocket();
        bool Bind();
        bool Lisent();
};
#endif
