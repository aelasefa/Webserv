#include "../../includes/Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <sstream>

#define BUFFER_SIZE 4096

Client::Client(int fd)
        : _fd(fd),
            _request(""),
            _isComplete(false),
            _contentLength(0),
            _responseBuffer(""),
            _bytesSent(0),
            _hasError(false),
            _errorCode(0),
            _closeAfterResponse(false),
            _lastActive(std::time(NULL))
{
}

bool Client::readData()
{
    if (_hasError)
        return true;

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
    touch();
    return true;
}

void Client::appendData(const std::string &buffer)
{
    if (_hasError)
        return;

    if (_request.size() + buffer.size() > MAX_REQUEST_SIZE)
    {
        setError(413);
        return;
    }

    _request += buffer;
}

bool Client::checkRequestComplete()
{
    if (_hasError)
        return true;

    size_t pos = _request.find("\r\n\r\n");
    if (pos == std::string::npos)
        return false;

    std::string headers = _request.substr(0, pos);
    std::string length_str;

    std::istringstream lines(headers);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        size_t sep = line.find(':');
        if (sep == std::string::npos)
            continue;

        std::string key = line.substr(0, sep);
        for (size_t i = 0; i < key.size(); ++i)
            key[i] = std::tolower(key[i]);

        if (key == "content-length")
        {
            length_str = line.substr(sep + 1);
            while (!length_str.empty() && length_str[0] == ' ')
                length_str.erase(0, 1);
            break;
        }
    }

    if (length_str.empty())
    {
        _isComplete = true;
        return true;
    }

    for (size_t i = 0; i < length_str.size(); ++i)
    {
        if (!std::isdigit(length_str[i]))
            return true;
    }

    _contentLength = static_cast<size_t>(std::atoi(length_str.c_str()));
    if (_contentLength > MAX_BODY_SIZE)
    {
        setError(413);
        return true;
    }

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
    touch();

    return sent;
}

bool Client::responseComplete() const
{
    return _bytesSent >= _responseBuffer.size();
}

void Client::setRequestBuffer(const std::string &buffer)
{
    _request = buffer;
}

bool Client::hasBufferedData() const
{
    return !_request.empty();
}

void Client::setError(int statusCode)
{
    _hasError = true;
    _errorCode = statusCode;
    _isComplete = true;
}

bool Client::hasError() const
{
    return _hasError;
}

int Client::getErrorCode() const
{
    return _errorCode;
}

void Client::setCloseAfterResponse(bool shouldClose)
{
    _closeAfterResponse = shouldClose;
}

bool Client::shouldCloseAfterResponse() const
{
    return _closeAfterResponse;
}

void Client::touch()
{
    _lastActive = std::time(NULL);
}

bool Client::isIdle(time_t now, int timeoutSec) const
{
    return (timeoutSec > 0) && (now - _lastActive >= timeoutSec);
}

void Client::reset()
{
    _request.clear();
    _responseBuffer.clear();

    _isComplete = false;
    _contentLength = 0;
    _bytesSent = 0;
    _hasError = false;
    _errorCode = 0;
    _closeAfterResponse = false;
}

void Client::resetForNextRequest(const std::string &remaining)
{
    _request = remaining;
    _responseBuffer.clear();
    _isComplete = false;
    _contentLength = 0;
    _bytesSent = 0;
    _hasError = false;
    _errorCode = 0;
    _closeAfterResponse = false;
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