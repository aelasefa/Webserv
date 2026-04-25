#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>

class Client
{
private:
    int _fd;
    std::string _request;
    bool _isComplete;
    size_t _contentLength;

public:
    Client(int fd);
    ~Client();

    void appendData(const std::string& buffer);
    bool checkRequestComplete();
    void reset();

    int getFd() const;
    const std::string& getRequest() const;
    bool isComplete() const;
};

#endif