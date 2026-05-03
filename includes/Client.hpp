#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <sys/types.h>

#define MAX_REQUEST_SIZE 8192

class Client
{
private:
    int _fd;

    std::string _request;
    bool _isComplete;
    size_t _contentLength;

    std::string _responseBuffer;
    size_t _bytesSent;

public:
    Client(int fd);
    ~Client();

    bool readData();
    void appendData(const std::string &buffer);

    bool checkRequestComplete();

    void setResponse(const std::string &response);
    ssize_t sendData();
    bool responseComplete() const;

    void reset();

    int getFd() const;
    const std::string &getRequest() const;
    size_t getBytesSent() const;
    size_t getResponseSize() const;
    bool isComplete() const;
};

#endif