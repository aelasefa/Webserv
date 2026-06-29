#include "../../includes/Request.hpp"
#include "../../includes/StringUtils.hpp"
#include "../../includes/HttpUtils.hpp"
#include <sstream>
#include <cstdlib>
#include <cerrno>
#include <limits>

std::string Request::getCookieValue(const std::string &cookieHeader, const std::string &key)
{
    if (cookieHeader.empty() || key.empty())
        return "";

    std::string search = key + "=";
    size_t pos = 0;

    while (pos < cookieHeader.size())
    {
        size_t found = cookieHeader.find(search, pos);
        if (found == std::string::npos)
            return "";

        bool atStart    = (found == 0);
        bool afterDelim = false;

        if (!atStart && found >= 1)
        {
            size_t back = found - 1;
            while (back > 0 && cookieHeader[back] == ' ')
                --back;
            if (cookieHeader[back] == ';')
                afterDelim = true;
            if (back == 0 && cookieHeader[0] == ';')
                afterDelim = true;
        }
        if (!atStart && found == 1 && cookieHeader[0] == ';')
            afterDelim = true;

        if (atStart || afterDelim)
        {
            size_t valStart = found + search.size();
            size_t end      = cookieHeader.find(';', valStart);
            if (end == std::string::npos)
                end = cookieHeader.size();

            std::string val = cookieHeader.substr(valStart, end - valStart);

            size_t vs = val.find_first_not_of(" \t");
            if (vs == std::string::npos)
                return "";
            size_t ve = val.find_last_not_of(" \t");
            return val.substr(vs, ve - vs + 1);
        }

        pos = found + 1;
    }

    return "";
}

Request::Request()
{
    _state             = START_LINE;
    _contentLength     = 0;
    _hasContentLength  = false;
    _hasTransferEncoding = false;
    _hasHost           = false;
    _isChunked         = false;
    _currentChunkSize  = 0;
    _chunkBytesRead    = 0;
    _errorStatus.clear();
    _shouldClose       = true;
    _queryString.clear();
    _maxBodySize       = std::numeric_limits<size_t>::max();
}

Request::~Request() {}

void Request::reset()
{
    _state             = START_LINE;
    _method.clear();
    _path.clear();
    _queryString.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _contentLength     = 0;
    _hasContentLength  = false;
    _hasTransferEncoding = false;
    _hasHost           = false;
    _isChunked         = false;
    _currentChunkSize  = 0;
    _chunkBytesRead    = 0;
    _errorStatus.clear();
    _shouldClose       = true;
    _maxBodySize       = std::numeric_limits<size_t>::max();
}

static bool isHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool parseChunkSizeLine(const std::string &line, size_t &sizeOut)
{
    std::string token = line;
    size_t semi = token.find(';');
    if (semi != std::string::npos)
        token = token.substr(0, semi);
    while (!token.empty() &&
           (token[token.size() - 1] == ' ' || token[token.size() - 1] == '\t'))
        token.erase(token.size() - 1);
    if (token.empty())
        return false;

    for (size_t i = 0; i < token.size(); i++)
    {
        if (!isHexDigit(token[i]))
            return false;
    }

    errno = 0;
    unsigned long value = std::strtoul(token.c_str(), 0, 16);
    if (errno != 0)
        return false;

    sizeOut = static_cast<size_t>(value);
    return true;
}

bool Request::isDone() const               { return _state == DONE; }
bool Request::hasError() const             { return _state == ERROR; }
std::string Request::getErrorStatus() const { return _errorStatus; }
bool Request::shouldClose() const          { return _shouldClose; }

std::string Request::getConnectionHeader() const
{
    return _shouldClose ? "close" : "keep-alive";
}

void Request::setMaxBodySize(size_t maxBodySize) { _maxBodySize = maxBodySize; }
size_t Request::getMaxBodySize() const           { return _maxBodySize; }

std::string Request::getMethod()      const { return _method; }
std::string Request::getPath()        const { return _path; }
std::string Request::getQueryString() const { return _queryString; }
std::string Request::getVersion()     const { return _version; }
std::string Request::getBody()        const { return _body; }

const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }

std::string Request::getHeader(const std::string& key) const
{
    std::string normalized = StringUtils::toLower(key);
    std::map<std::string, std::string>::const_iterator it = _headers.find(normalized);
    if (it == _headers.end())
        return "";
    return it->second;
}

bool Request::parse(std::string &buffer)
{
    while (true)
    {
        if (_state == START_LINE)
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                return false;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            if (!parseStartLine(line))
            {
                _state = ERROR;
                if (_errorStatus.empty())
                    _errorStatus = "400 Bad Request";
                return false;
            }

            _state = HEADERS;
        }
        else if (_state == HEADERS)
        {
            if (!parseHeaders(buffer))
                return false;
        }
        else if (_state == BODY)
        {
            if (!parseBody(buffer))
                return false;
        }
        else if (_state == CHUNK_SIZE || _state == CHUNK_DATA ||
                 _state == CHUNK_DATA_CRLF || _state == CHUNK_TRAILER)
        {
            if (!parseChunkedBody(buffer))
                return false;
        }
        else if (_state == DONE)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}

bool Request::parseStartLine(std::string &line)
{
    std::istringstream ss(line);

    std::string rawPath;
    ss >> _method >> rawPath >> _version;

    std::string extra;
    if (ss >> extra)
        return false;

    if (_method.empty() || rawPath.empty() || _version.empty())
        return false;

    size_t queryPos = rawPath.find('?');
    if (queryPos == std::string::npos)
    {
        _path = StringUtils::urlDecode(rawPath);
        _queryString.clear();
    }
    else
    {
        _path        = StringUtils::urlDecode(rawPath.substr(0, queryPos));
        _queryString = rawPath.substr(queryPos + 1);
    }

    if (_path.empty())
        return false;

    if (!HttpUtils::isValidMethod(_method))
    {
        _errorStatus = "400 Bad Request";
        return false;
    }

    if (!HttpUtils::isValidHttpVersion(_version))
    {
        _errorStatus = "400 Bad Request";
        return false;
    }

    _shouldClose = (_version != "HTTP/1.1");

    return true;
}

bool Request::parseHeaders(std::string &buffer)
{
    static const size_t MAX_HEADER_COUNT = 100;
    static const size_t MAX_HEADER_LINE  = 8192;
    size_t headerCount = 0;

    while (true)
    {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos)
            return false;
        if (pos > MAX_HEADER_LINE)
        {
            _errorStatus = "431 Request Header Fields Too Large";
            _state       = ERROR;
            return false;
        }

        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (line.empty())
        {
            if (_hasContentLength && _hasTransferEncoding)
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }

            if (_hasTransferEncoding && _isChunked)
            {
                _contentLength    = 0;
                _hasContentLength = false;
            }

            if (_hasContentLength && _isChunked)
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }

            if (_hasContentLength)
            {
                std::string cl = _headers["content-length"];
                if (!StringUtils::isNumeric(cl))
                {
                    _errorStatus = "400 Bad Request";
                    _state       = ERROR;
                    return false;
                }

                errno = 0;
                unsigned long len = std::strtoul(cl.c_str(), 0, 10);
                if (errno != 0)
                {
                    _errorStatus = "400 Bad Request";
                    _state       = ERROR;
                    return false;
                }

                if (len > _maxBodySize)
                {
                    _errorStatus = "413 Payload Too Large";
                    _state       = ERROR;
                    return false;
                }

                _contentLength = static_cast<size_t>(len);
            }

            if (_method == "POST" && !_hasContentLength && !_isChunked)
            {
                _errorStatus = "411 Length Required";
                _state       = ERROR;
                return false;
            }

            if (_version == "HTTP/1.1" && _headers.find("host") == _headers.end())
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }

            if (_headers.find("connection") != _headers.end())
            {
                std::string conn = StringUtils::toLower(_headers["connection"]);
                if (conn.find("close") != std::string::npos)
                    _shouldClose = true;
                else if (conn.find("keep-alive") != std::string::npos)
                    _shouldClose = false;
            }

            _state = _isChunked ? CHUNK_SIZE : BODY;
            return true;
        }

        if (++headerCount > MAX_HEADER_COUNT)
        {
            _errorStatus = "431 Request Header Fields Too Large";
            _state       = ERROR;
            return false;
        }

        size_t sep = line.find(":");
        if (sep == std::string::npos)
        {
            _errorStatus = "400 Bad Request";
            _state       = ERROR;
            return false;
        }

        std::string key   = StringUtils::toLower(StringUtils::trim(line.substr(0, sep)));
        std::string value = StringUtils::trim(line.substr(sep + 1));

        if (key.empty())
        {
            _errorStatus = "400 Bad Request";
            _state       = ERROR;
            return false;
        }

        if (key == "host")
        {
            if (_hasHost)
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }
            _hasHost = true;
        }

        if (key == "content-length")
        {
            if (_hasContentLength)
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }
            _hasContentLength = true;
        }

        if (key == "transfer-encoding")
        {
            _hasTransferEncoding = true;
            std::string val = StringUtils::toLower(value);
            std::vector<std::string> tokens = StringUtils::split(val, ',');
            bool hasChunked = false;

            for (size_t i = 0; i < tokens.size(); i++)
            {
                std::string token = StringUtils::trim(tokens[i]);
                if (token.empty() || token == "identity")
                    continue;
                if (token == "chunked")
                {
                    hasChunked = true;
                    continue;
                }

                _errorStatus = "501 Not Implemented";
                _state       = ERROR;
                return false;
            }

            _isChunked = hasChunked;
        }

        _headers[key] = value;
    }
}

bool Request::parseBody(std::string &buffer)
{
    if (_isChunked)
        return parseChunkedBody(buffer);

    if (_contentLength > _maxBodySize)
    {
        _errorStatus = "413 Payload Too Large";
        _state       = ERROR;
        return false;
    }

    size_t needed = _contentLength - _body.size();

    if (buffer.size() < needed)
    {
        _body.append(buffer);
        buffer.clear();
        return false;
    }

    _body.append(buffer.substr(0, needed));
    buffer.erase(0, needed);

    _state = DONE;
    return true;
}

bool Request::parseChunkedBody(std::string &buffer)
{
    while (true)
    {
        if (_state == CHUNK_SIZE)
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                return false;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            size_t chunkSize = 0;
            if (!parseChunkSizeLine(line, chunkSize))
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }

            _currentChunkSize = chunkSize;
            _chunkBytesRead   = 0;

            _state = (_currentChunkSize == 0) ? CHUNK_TRAILER : CHUNK_DATA;
        }
        else if (_state == CHUNK_DATA)
        {
            size_t remaining = _currentChunkSize - _chunkBytesRead;
            if (buffer.size() < remaining)
            {
                if (_body.size() + buffer.size() > _maxBodySize)
                {
                    _errorStatus = "413 Payload Too Large";
                    _state       = ERROR;
                    return false;
                }
                _body.append(buffer);
                _chunkBytesRead += buffer.size();
                buffer.clear();
                return false;
            }

            if (_body.size() + remaining > _maxBodySize)
            {
                _errorStatus = "413 Payload Too Large";
                _state       = ERROR;
                return false;
            }

            _body.append(buffer.substr(0, remaining));
            buffer.erase(0, remaining);
            _chunkBytesRead += remaining;
            _state = CHUNK_DATA_CRLF;
        }
        else if (_state == CHUNK_DATA_CRLF)
        {
            if (buffer.size() < 2)
                return false;
            if (buffer[0] != '\r' || buffer[1] != '\n')
            {
                _errorStatus = "400 Bad Request";
                _state       = ERROR;
                return false;
            }
            buffer.erase(0, 2);
            _state = CHUNK_SIZE;
        }
        else if (_state == CHUNK_TRAILER)
        {
            size_t pos = buffer.find("\r\n");
            if (pos == std::string::npos)
                return false;

            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            if (line.empty())
            {
                _state = DONE;
                return true;
            }
        }
        else
        {
            return false;
        }
    }
}