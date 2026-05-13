#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <sys/types.h>
#include <ctime>
#include "Request.hpp"

#define MAX_REQUEST_SIZE (MAX_BODY_SIZE + 8192)

class Client
{
private:
    int _fd;

    std::string _request;
    bool _isComplete;
    size_t _contentLength;

    std::string _responseBuffer;
    size_t _bytesSent;

    bool _hasError;
    int _errorCode;
    bool _closeAfterResponse;
    time_t _lastActive;

public:
    Client(int fd);
    ~Client();

    bool readData();
    void appendData(const std::string &buffer);

    bool checkRequestComplete();

    void setResponse(const std::string &response);
    ssize_t sendData();
    bool responseComplete() const;

    void setRequestBuffer(const std::string &buffer);
    bool hasBufferedData() const;

    void setError(int statusCode);
    bool hasError() const;
    int getErrorCode() const;

    void setCloseAfterResponse(bool shouldClose);
    bool shouldCloseAfterResponse() const;

    void touch();
    bool isIdle(time_t now, int timeoutSec) const;

    void reset();
    void resetForNextRequest(const std::string &remaining);

    int getFd() const;
    const std::string &getRequest() const;
    size_t getBytesSent() const;
    size_t getResponseSize() const;
    bool isComplete() const;
};

#endif