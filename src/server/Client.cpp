#include "../../includes/Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cerrno>

#define BUFFER_SIZE 4096

Client::Client(int fd)
    : _fd(fd),
      _request(""),
      _isComplete(false),
      _contentLength(0),
      _responseBuffer(""),
      _bytesSent(0)
{
}

bool Client::readData()
{
    char buffer[BUFFER_SIZE];

    ssize_t bytes = recv(_fd, buffer, BUFFER_SIZE, 0);
    if (bytes < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true;
        return false;
    }
    if (bytes == 0)
        return false;

    appendData(std::string(buffer, bytes));
    return true;
}

void Client::appendData(const std::string &buffer)
{
    if (_request.size() + buffer.size() > MAX_REQUEST_SIZE)
    {
        _isComplete = true; // reject oversized request
        return;
    }

    _request += buffer;
}

bool Client::checkRequestComplete()
{
    size_t pos = _request.find("\r\n\r\n");
    if (pos == std::string::npos)
        return false;

    std::string headers = _request.substr(0, pos);

    size_t cl_pos = headers.find("Content-Length:");

    if (cl_pos == std::string::npos)
    {
        _isComplete = true;
        return true;
    }

    size_t start = cl_pos + 15;

    while (start < headers.size() && headers[start] == ' ')
        start++;

    size_t end = headers.find("\r\n", start);
    if (end == std::string::npos)
        return false;

    std::string length_str = headers.substr(start, end - start);
    if (length_str.empty())
        return false;

    _contentLength = std::atoi(length_str.c_str());

    size_t body_start = pos + 4;
    size_t body_size = _request.size() - body_start;

    if (body_size >= _contentLength)
    {
        _isComplete = true;
        return true;
    }

    return false;
}

void Client::setResponse(const std::string &response)
{
    _responseBuffer = response;
    _bytesSent = 0;
}

ssize_t Client::sendData()
{
    if (_responseBuffer.empty())
        return 0;

    ssize_t sent = send(_fd,
                        _responseBuffer.c_str() + _bytesSent,
                        _responseBuffer.size() - _bytesSent,
                        0);

    if (sent < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }

    _bytesSent += sent;

    return sent;
}

bool Client::responseComplete() const
{
    return _bytesSent >= _responseBuffer.size();
}

void Client::reset()
{
    _request.clear();
    _responseBuffer.clear();

    _isComplete = false;
    _contentLength = 0;
    _bytesSent = 0;
}

int Client::getFd() const
{
    return _fd;
}

const std::string &Client::getRequest() const
{
    return _request;
}

size_t Client::getBytesSent() const
{
    return _bytesSent;
}

size_t Client::getResponseSize() const
{
    return _responseBuffer.size();
}

bool Client::isComplete() const
{
    return _isComplete;
}


Client::~Client() {}