#include "../../includes/Client.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <limits>

#define BUFFER_SIZE 4096
#define FILE_CHUNK_SIZE 262144    
#define SEND_BUDGET_PER_CALL (1024 * 1024) 

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
            _responseFileFd(-1),
            _responseFileRemaining(0),
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

    if (_request.size() + buffer.size() > _maxBodySize)
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
    closeResponseFile();
    _responseBuffer = response;
    _bytesSent = 0;
}

bool Client::setResponseHeaderAndFile(const std::string &headerBlock,
                                       const std::string &filePath,
                                       size_t fileSize)
{
    closeResponseFile();

    _responseBuffer = headerBlock;
    _bytesSent = 0;

    _responseFileFd = open(filePath.c_str(), O_RDONLY);
    if (_responseFileFd < 0)
    {
        _responseFileRemaining = 0;
        return false;
    }

    _responseFileRemaining = fileSize;
    return true;
}

void Client::closeResponseFile()
{
    if (_responseFileFd >= 0)
    {
        close(_responseFileFd);
        _responseFileFd = -1;
    }
    _responseFileRemaining = 0;
}

bool Client::loadNextFileChunk()
{
    if (_responseFileFd < 0)
        return false;

    if (_responseFileRemaining == 0)
    {
        closeResponseFile();
        return false;
    }

    char buf[FILE_CHUNK_SIZE];
    size_t toRead = (_responseFileRemaining < (size_t)FILE_CHUNK_SIZE)
                        ? _responseFileRemaining
                        : (size_t)FILE_CHUNK_SIZE;

    ssize_t got = read(_responseFileFd, buf, toRead);
    if (got <= 0)
    {
        // Read error or the file shrank/disappeared mid-stream: stop here,
        // the client will just see a short body / connection close.
        closeResponseFile();
        return false;
    }

    _responseBuffer.assign(buf, (size_t)got);
    _bytesSent = 0;
    _responseFileRemaining -= (size_t)got;

    return true;
}

ssize_t Client::sendData()
{
    size_t totalSent = 0;

    while (totalSent < (size_t)SEND_BUDGET_PER_CALL)
    {
        if (_bytesSent >= _responseBuffer.size())
        {
            if (!loadNextFileChunk())
                break;         }

        size_t toSend = _responseBuffer.size() - _bytesSent;
        ssize_t sent = send(_fd, _responseBuffer.c_str() + _bytesSent, toSend, MSG_NOSIGNAL);

        if (sent < 0)
        {
            if (totalSent > 0)
                break;
            return -1;
        }

        _bytesSent += (size_t)sent;
        totalSent += (size_t)sent;
        touch();

        if ((size_t)sent < toSend)
            break; // short write: socket send buffer is full right now,
                    // stop and let poll() tell us when it's writable again
    }

    return (ssize_t)totalSent;
}

bool Client::responseComplete() const
{
    if (_bytesSent < _responseBuffer.size())
        return false;
    return _responseFileFd < 0;
}

bool Client::hasResponse() const
{
    return _bytesSent < _responseBuffer.size() || _responseFileFd >= 0;
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
    closeResponseFile();
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
    closeResponseFile();
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


Client::~Client()
{
    closeResponseFile();
}