#include "../../includes/Client.hpp"
#include <cstdlib>

Client::Client(int fd)
    : _fd(fd), _request(""), _isComplete(false), _contentLength(0) {}

Client::~Client() {}

void Client::appendData(const std::string &buffer)
{
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
    std::string length_str = headers.substr(start, end - start);

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

void Client::reset()
{
    _request.clear();
    _isComplete = false;
    _contentLength = 0;
}

int Client::getFd() const
{
    return _fd;
}

const std::string& Client::getRequest() const
{
    return _request;
}

bool Client::isComplete() const
{
    return _isComplete;
}