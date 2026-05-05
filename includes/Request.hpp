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
        DONE,
        ERROR
    };

private:
    ParseState _state;

    std::string _method;
    std::string _path;
    std::string _version;

    std::map<std::string, std::string> _headers;
    std::string _body;

    size_t _contentLength;
    std::string _errorStatus;
    bool _shouldClose;

public:
    Request();
    ~Request();

    bool parse(std::string &buffer);

    bool parseStartLine(std::string &line);
    bool parseHeaders(std::string &buffer);
    bool parseBody(std::string &buffer);

    bool isDone() const;
    bool hasError() const;
    void reset();

    std::string getErrorStatus() const;
    bool shouldClose() const;
    std::string getConnectionHeader() const;

    std::string getMethod() const;
    std::string getPath() const;
    std::string getVersion() const;
    std::string getBody() const;
    const std::map<std::string, std::string>& getHeaders() const;
};

#endif