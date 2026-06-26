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
            _peerClosed(false),
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
            _lastActive(std::time(NULL)),
            _cgiRunning(false),
            _cgiShouldClose(false)
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
    {
        _peerClosed = true;
        return true;
    }

    appendData(std::string(buffer, bytes));
    _lastActive = std::time(NULL);
    return true;
}

void Client::appendData(const std::string &buffer)
{
    if (_hasError)
        return;

    if (_request.find("\r\n\r\n") == std::string::npos)
    {
        std::string combined = _request + buffer;
        size_t boundary = combined.find("\r\n\r\n");
        if (boundary == std::string::npos)
        {
            if (combined.size() > 8192)
            {
                setError(431);
                return;
            }
        }
        else
        {
            if (boundary > 8192)
            {
                setError(431);
                return;
            }
        }
    }

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
    _lastActive = std::time(NULL);

        if ((size_t)sent < toSend)
            break; 
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


bool Client::isIdle(time_t now, int timeoutSec) const
{
    return (timeoutSec > 0) && (now - _lastActive >= timeoutSec);
}

void Client::reset()
{
    closeResponseFile();
    clearCgi();
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
    clearCgi();
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


// ---------- CGI integration ----------

void Client::startCgi(const std::string &scriptPath, const std::string &interpreter,
                      const Request &req, bool shouldClose)
{
    _cgi.setScriptPath(scriptPath);
    _cgi.setInterpreter(interpreter);
    _cgiState = CGIState();
    _cgi.start(req, _cgiState);
    _cgiRunning = true;
    _cgiShouldClose = shouldClose;
}

bool Client::isCgiRunning() const
{
    return _cgiRunning;
}

bool Client::updateCgi()
{
    if (!_cgiRunning)
        return true;
    try
    {
        if (_cgi.update(_cgiState))
        {
            _cgiRunning = false;
            return true;
        }
        return false;
    }
    catch (const std::exception &)
    {
        _cgiRunning = false;
        _cgiState.result.clear();
        return true;
    }
}

CGIState &Client::getCgiState()
{
    return _cgiState;
}

bool Client::getCgiShouldClose() const
{
    return _cgiShouldClose;
}

void Client::clearCgi()
{
    if (_cgiRunning && _cgiState.running)
    {
        kill(_cgiState.pid, SIGKILL);
        waitpid(_cgiState.pid, NULL, 0);
        if (_cgiState.outputFd >= 0)
            close(_cgiState.outputFd);
    }
    _cgiRunning = false;
    _cgiState = CGIState();
}

Client::~Client()
{
    clearCgi();
    closeResponseFile();
}