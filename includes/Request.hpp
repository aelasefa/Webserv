#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

#define MAX_BODY_SIZE 1048576

class Request
{
public:
    enum ParseState
    {
        START_LINE,
        HEADERS,
        BODY,
        CHUNK_SIZE,
        CHUNK_DATA,
        CHUNK_DATA_CRLF,
        CHUNK_TRAILER,
        DONE,
        ERROR
    };

private:
    ParseState _state;

    std::string _method;
    std::string _path;
    std::string _queryString;
    std::string _version;

    std::map<std::string, std::string> _headers;
    std::string _body;

    size_t _contentLength;
    bool _hasContentLength;
    bool _hasTransferEncoding;
    bool _hasHost;
    bool _isChunked;
    size_t _currentChunkSize;
    size_t _chunkBytesRead;
    std::string _errorStatus;
    bool _shouldClose;
    size_t _maxBodySize;

public:
    Request();
    ~Request();

    bool parse(std::string &buffer);

    bool parseStartLine(std::string &line);
    bool parseHeaders(std::string &buffer);
    bool parseBody(std::string &buffer);
    bool parseChunkedBody(std::string &buffer);

    bool isDone() const;
    bool hasError() const;
    void reset();
    static std::string getCookieValue(const std::string &cookieHeader, const std::string &key);

    std::string getErrorStatus() const;
    bool shouldClose() const;
    std::string getConnectionHeader() const;
    void setMaxBodySize(size_t maxBodySize);
    size_t getMaxBodySize() const;

    std::string getMethod() const;
    std::string getPath() const;
    std::string getQueryString() const;
    std::string getVersion() const;
    std::string getBody() const;
    const std::map<std::string, std::string>& getHeaders() const;
    std::string getHeader(const std::string& key) const;
};

#endif   