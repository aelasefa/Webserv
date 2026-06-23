#include "../../includes/Response.hpp"
#include <sstream>

#include <algorithm>
#include <cctype>

void Response::addCookie(const std::string &cookie)
{
    _cookies.push_back(cookie);
}

std::string makeSetCookie(const std::string &key, const std::string &value)
{
    return key + "=" + value + "; Path=/";
}

std::string intToString(size_t value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

Response::Response() : statusCode(200),
        statusMessage("OK"), body(""),
        _isFileBody(false), _filePath(""), _fileSize(0) {}

void Response::setStatus(int status)
{
    if (status < 100 || status > 599)
    {
        statusCode = 500;
        statusMessage = "Internal Server Error";
        return;
    }

    statusCode = status;

    switch (statusCode)
    {
        case 200: statusMessage = "OK"; break;
        case 201: statusMessage = "Created"; break;
        case 204: statusMessage = "No Content"; break;
        case 301: statusMessage = "Moved Permanently"; break;
        case 302: statusMessage = "Found"; break;
        case 303: statusMessage = "See Other"; break;
        case 307: statusMessage = "Temporary Redirect"; break;
        case 308: statusMessage = "Permanent Redirect"; break;
        case 400: statusMessage = "Bad Request"; break;
        case 403: statusMessage = "Forbidden"; break;
        case 404: statusMessage = "Not Found"; break;
        case 405: statusMessage = "Method Not Allowed"; break;
        case 408: statusMessage = "Request Timeout"; break;
        case 409: statusMessage = "Conflict"; break;
        case 411: statusMessage = "Length Required"; break;
        case 413: statusMessage = "Payload Too Large"; break;
        case 500: statusMessage = "Internal Server Error"; break;
        case 501: statusMessage = "Not Implemented"; break;
        case 502: statusMessage = "Bad Gateway"; break;
        default:  statusMessage = "Unknown"; break;
    }
}

int Response::getStatusCode() const {
    return statusCode;
}

void Response::setHeader(const std::string& key, const std::string& value) {
    headers.push_back(std::make_pair(key, value));
}

void Response::setBody(const std::string& content) {
    body = content;
    _isFileBody = false;
    _filePath.clear();
    _fileSize = 0;
}

void Response::setFileBody(const std::string &path, size_t fileSize)
{
    _isFileBody = true;
    _filePath = path;
    _fileSize = fileSize;
    body.clear();
}

bool Response::isFileBody() const
{
    return _isFileBody;
}

const std::string &Response::getFilePath() const
{
    return _filePath;
}
const std::string &Response::getBody() const
{
    return body;
}

size_t Response::getFileSize() const
{
    return _fileSize;
}

std::string Response::buildHeaderBlock(size_t contentLength) const {
    std::string result;
    result += "HTTP/1.1 ";
    result += intToString(statusCode);
    result += " ";
    result += statusMessage;
    result += "\r\n";
    bool hasContentType = false;
    bool hasContentLength = false;
    for (std::vector< std::pair<std::string, std::string> >::const_iterator it = headers.begin(); it != headers.end(); ++it)
    {
        std::string keyLower = it->first;
        for (size_t i = 0; i < keyLower.size(); ++i)
            keyLower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(keyLower[i])));
        if (keyLower == "content-type")
            hasContentType = true;
        if (keyLower == "content-length")
            hasContentLength = true;
    }
    if (!hasContentType)
        result += std::string("Content-Type: text/plain\r\n");
    if (!hasContentLength)
        result += std::string("Content-Length: ") + intToString(contentLength) + "\r\n";
    for (std::vector< std::pair<std::string, std::string> >::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        result += it->first + ": " + it->second + "\r\n";
    }
    result += "\r\n";
    return result;
}

std::string Response::build() const {
    std::string result = buildHeaderBlock(_isFileBody ? _fileSize : body.size());
    if (!_isFileBody)
        result += body;
    return result;
}

std::string Response::buildHeaders() const {
    return buildHeaderBlock(_isFileBody ? _fileSize : body.size());
}