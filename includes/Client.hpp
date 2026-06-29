#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <cstddef>
#include <sys/types.h>
#include <ctime>
#include "Request.hpp"
#include "CGI.hpp"


class Server;

class Client
{
private:
    int _fd;
    size_t _serverIndex;
    bool _peerClosed;

    std::string _request;
    bool _isComplete;
    size_t _contentLength;
    Request _parser;
    size_t _maxBodySize;

    std::string _responseBuffer;
    size_t _bytesSent;

    int _responseFileFd;
    size_t _responseFileRemaining;

    bool _hasError;
    int _errorCode;
    bool _closeAfterResponse;
    time_t _lastActive;

    // --- CGI state ---
    bool _cgiRunning;
    CGI _cgi;
    CGIState _cgiState;
    bool _cgiShouldClose;

    void closeResponseFile();
    bool loadNextFileChunk();

public:
    Client(int fd, size_t serverIndex);
    ~Client();

    bool readData();
    void appendData(const std::string &buffer);

    bool checkRequestComplete();

    void setResponse(const std::string &response);
    bool setResponseHeaderAndFile(const std::string &headerBlock,
                                   const std::string &filePath,
                                   size_t fileSize);
    ssize_t sendData();
    bool responseComplete() const;
    bool hasResponse() const;
    bool isPeerClosed() const { return _peerClosed; }

    void setRequestBuffer(const std::string &buffer);
    bool hasBufferedData() const;
    std::string &getRequestBuffer();
    Request &getParser();
    void setMaxBodySize(size_t maxBodySize);
    size_t getMaxBodySize() const;

    void setError(int statusCode);
    bool hasError() const;
    int getErrorCode() const;

    void setCloseAfterResponse(bool shouldClose);
    bool shouldCloseAfterResponse() const;

    bool isIdle(time_t now, int timeoutSec) const;

    void reset();
    void resetForNextRequest(const std::string &remaining);

    int getFd() const;
    size_t getServerIndex() const;
    const std::string &getRequest() const;
    size_t getBytesSent() const;
    size_t getResponseSize() const;
    bool isComplete() const;
    time_t getLastActivityTime() const { return _lastActive; }
    void updateLastActivityTime() { _lastActive = std::time(NULL); }

    // --- CGI methods ---
    void startCgi(const std::string &scriptPath, const std::string &interpreter,
                  const Request &req, bool shouldClose, const Server &server);
    bool isCgiRunning() const;
    bool updateCgi();
    CGIState &getCgiState();
    bool getCgiShouldClose() const;
    void clearCgi();
};

#endif