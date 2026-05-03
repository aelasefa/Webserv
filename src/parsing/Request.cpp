#include "../../includes/Request.hpp"
#include "../../includes/Utils.hpp"
#include <sstream>
#include <cstdlib>
#include <cerrno>

Request::Request()
{
    _state = START_LINE;
    _contentLength = 0;
    _errorStatus.clear();
    _shouldClose = true;
}

Request::~Request() {}

void Request::reset()
{
    _state = START_LINE;
    _method.clear();
    _path.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _contentLength = 0;
    _errorStatus.clear();
    _shouldClose = true;
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

std::string Request::getMethod() const { return _method; }
std::string Request::getPath() const { return _path; }
std::string Request::getVersion() const { return _version; }
std::string Request::getBody() const { return _body; }
const std::map<std::string, std::string>& Request::getHeaders() const { return _headers; }

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

    ss >> _method >> _path >> _version;

    if (_method.empty() || _path.empty() || _version.empty())
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
            if (_headers.find("content-length") != _headers.end())
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

                if (len > MAX_BODY_SIZE)
                {
                    _errorStatus = "413 Payload Too Large";
                    _state = ERROR;
                    return false;
                }

                _contentLength = static_cast<size_t>(len);
            }
            else
            {
                _contentLength = 0;
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

        _headers[key] = value;
    }
}

bool Request::parseBody(std::string &buffer)
{
    if (_contentLength > MAX_BODY_SIZE)
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