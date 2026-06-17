#include "../../includes/Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <limits>

#define BUFFER_SIZE 4096

Client::Client(int fd, size_t serverIndex)
        : _fd(fd),
            _serverIndex(serverIndex),
            _request(""),
            _isComplete(false),
            _contentLength(0),
            _parser(),
            _maxBodySize(std::numeric_limits<size_t>::max()),
            _responseBuffer(""),
            _bytesSent(0),
            _hasError(false),
            _errorCode(0),
            _closeAfterResponse(false),
            _lastActive(std::time(NULL))
{
        _parser.setMaxBodySize(_maxBodySize);
}

bool Client::readData()
{
    if (_hasError)
        return true;

    char buffer[BUFFER_SIZE];

    ssize_t bytes = recv(_fd, buffer, BUFFER_SIZE, 0);
    if (bytes < 0)
        return false;
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

    if (_request.size() > _maxBodySize - buffer.size())
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

    return _request.find("\r\n\r\n") != std::string::npos;
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
        return -1;

    _bytesSent += sent;
    touch();

    return sent;
}

bool Client::responseComplete() const
{
    return _bytesSent >= _responseBuffer.size();
}

bool Client::hasResponse() const
{
    return !_responseBuffer.empty();
}

void Client::setRequestBuffer(const std::string &buffer)
{
    _request = buffer;
}

bool Client::hasBufferedData() const
{
    return !_request.empty();
}

std::string &Client::getRequestBuffer()
{
    return _request;
}

Request &Client::getParser()
{
    return _parser;
}

void Client::setMaxBodySize(size_t maxBodySize)
{
    _maxBodySize = maxBodySize;
    _parser.setMaxBodySize(_maxBodySize);
}

size_t Client::getMaxBodySize() const
{
    return _maxBodySize;
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
    _parser.reset();
    _parser.setMaxBodySize(_maxBodySize);
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
    _parser.reset();
    _parser.setMaxBodySize(_maxBodySize);
}

int Client::getFd() const
{
    return _fd;
}

size_t Client::getServerIndex() const
{
    return _serverIndex;
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