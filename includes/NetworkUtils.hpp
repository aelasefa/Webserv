#ifndef NETWORKUTILS_HPP
#define NETWORKUTILS_HPP

class NetworkUtils
{
public:
    static int setNonBlocking(int fd);
    static int createServerSocket(int port);
};

#endif
