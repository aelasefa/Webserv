#include "../../includes/Request.hpp"
#include "../../includes/Utils.hpp"
#include <sstream>
#include <cstdlib>
#include <cerrno>
#include <limits>

std::string Request::getCookieValue(const std::string &cookieHeader, const std::string &key)
{
    size_t pos = cookieHeader.find(key + "=");
    if (pos == std::string::npos)
        return "";

    pos += key.size() + 1;

    size_t end = cookieHeader.find(';', pos);
    if (end == std::string::npos)
        end = cookieHeader.size();

    return cookieHeader.substr(pos, end - pos);
}

Request::Request()
{
    _state = START_LINE;
    _contentLength = 0;
    _hasContentLength = false;
    _hasTransferEncoding = false;
    _hasHost = false;
    _isChunked = false;
    _currentChunkSize = 0;
    _chunkBytesRead = 0;
    _errorStatus.clear();
    _shouldClose = true;
    _queryString.clear();
    _maxBodySize = std::numeric_limits<size_t>::max();
}

Request::~Request() {}

void Request::reset()
{
    _state = START_LINE;
    _method.clear();
    _path.clear();
    _queryString.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _contentLength = 0;
    _hasContentLength = false;
    _hasTransferEncoding = false;
    _hasHost = false;
    _isChunked = false;
    _currentChunkSize = 0;
    _chunkBytesRead = 0;
    _errorStatus.clear();
    _shouldClose = true;
    _maxBodySize = std::numeric_limits<size_t>::max();
}

namespace
{
    bool isHexDigit(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    bool parseChunkSizeLine(const std::string &line, size_t &sizeOut)
    {
        std::string token = line;
        size_t semi = token.find(';');
        if (semi != std::string::npos)
            token = token.substr(0, semi);
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
}

bool Request::isDone() const
{
    return _state == DONE;
}

bool Request::hasError() const
{
    return _state == ERROR;
}

std::string Request::getErrorStatus() const
{
    return _errorStatus;
}

bool Request::shouldClose() const
{
    return _shouldClose;
}

std::string Request::getConnectionHeader() const
{
    return _shouldClose ? "close" : "keep-alive";
}

void Request::setMaxBodySize(size_t maxBodySize)
{
        _maxBodySize = maxBodySize;
}

size_t Request::getMaxBodySize() const
{
    return _maxBodySize;
}

std::string Request::getMethod() const { return _method; }
std::string Request::getPath() const { return _path; }
std::string Request::getQueryString() const { return _queryString; }
std::string Request::getVersion() const { return _version; }
std::string Request::getBody() const { return _body; }
const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }
std::string Request::getHeader(const std::string& key) const
{
    std::string normalized = Utils::toLower(key);
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

        else if (_state == CHUNK_SIZE || _state == CHUNK_DATA || _state == CHUNK_DATA_CRLF || _state == CHUNK_TRAILER)
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
        _path = rawPath;
        _queryString.clear();
    }
    else
    {
        _path = rawPath.substr(0, queryPos);
        _queryString = rawPath.substr(queryPos + 1);
    }

    if (_path.empty())
        return false;

    if (!Utils::isValidMethod(_method))
    {
        _errorStatus = "400 Bad Request";
        return false;
    }

    if (!Utils::isValidHttpVersion(_version))
    {
        _errorStatus = "400 Bad Request";
        return false;
    }

    if (_version == "HTTP/1.1")
        _shouldClose = false;
    else
        _shouldClose = true;

    return true;
}

bool Request::parseHeaders(std::string &buffer)
{
    while (true)
    {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos)
            return false;

        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        if (line.empty())
        {
            if (_hasTransferEncoding && _isChunked)
            {
                _contentLength = 0;
                _hasContentLength = false;
            }

            if (_hasContentLength && _isChunked)
            {
                _errorStatus = "400 Bad Request";
                _state = ERROR;
                return false;
            }

            if (_hasContentLength)
            {
                std::string cl = _headers["content-length"];
                if (!Utils::isNumeric(cl))
                {
                    _errorStatus = "400 Bad Request";
                    _state = ERROR;
                    return false;
                }

                errno = 0;
                unsigned long len = std::strtoul(cl.c_str(), 0, 10);
                if (errno != 0)
                {
                    _errorStatus = "400 Bad Request";
                    _state = ERROR;
                    return false;
                }

                if (len > _maxBodySize)
                {
                    _errorStatus = "413 Payload Too Large";
                    _state = ERROR;
                    return false;
                }

                _contentLength = static_cast<size_t>(len);
            }

            if (_method == "POST" && !_hasContentLength && !_isChunked)
            {
                _errorStatus = "411 Length Required";
                _state = ERROR;
                return false;
            }

            if (_version == "HTTP/1.1" && _headers.find("host") == _headers.end())
            {
                _errorStatus = "400 Bad Request";
                _state = ERROR;
                return false;
            }

            if (_headers.find("connection") != _headers.end())
            {
                std::string conn = Utils::toLower(_headers["connection"]);
                if (conn.find("close") != std::string::npos)
                    _shouldClose = true;
                else if (conn.find("keep-alive") != std::string::npos)
                    _shouldClose = false;
            }

            if (_isChunked)
                _state = CHUNK_SIZE;
            else
                _state = BODY;
            return true;
        }

        size_t sep = line.find(":");
        if (sep == std::string::npos)
        {
            _errorStatus = "400 Bad Request";
            _state = ERROR;
            return false;
        }

        std::string key = Utils::toLower(Utils::trim(line.substr(0, sep)));
        std::string value = Utils::trim(line.substr(sep + 1));

        if (key.empty())
        {
            _errorStatus = "400 Bad Request";
            _state = ERROR;
            return false;
        }

        if (key == "host")
        {
            if (_hasHost)
            {
                _errorStatus = "400 Bad Request";
                _state = ERROR;
                return false;
            }
            _hasHost = true;
        }

        if (key == "content-length")
        {
            if (_hasContentLength)
            {
                _errorStatus = "400 Bad Request";
                _state = ERROR;
                return false;
            }
            _hasContentLength = true;
        }

        if (key == "transfer-encoding")
        {
            _hasTransferEncoding = true;
            std::string val = Utils::toLower(value);
            std::vector<std::string> tokens = Utils::split(val, ',');
            bool hasChunked = false;

            for (size_t i = 0; i < tokens.size(); i++)
            {
                std::string token = Utils::trim(tokens[i]);
                if (token.empty() || token == "identity")
                    continue;
                if (token == "chunked")
                {
                    hasChunked = true;
                    continue;
                }

                _errorStatus = "501 Not Implemented";
                _state = ERROR;
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
        _state = ERROR;
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
                _state = ERROR;
                return false;
            }

            _currentChunkSize = chunkSize;
            _chunkBytesRead = 0;

            if (_currentChunkSize == 0)
            {
                _state = CHUNK_TRAILER;
            }
            else
            {
                _state = CHUNK_DATA;
            }
        }
        else if (_state == CHUNK_DATA)
        {
            size_t remaining = _currentChunkSize - _chunkBytesRead;
            if (buffer.size() < remaining)
            {
                if (_body.size() + buffer.size() > _maxBodySize)
                {
                    _errorStatus = "413 Payload Too Large";
                    _state = ERROR;
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
                _state = ERROR;
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
                _state = ERROR;
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