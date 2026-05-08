#include "../../includes/Response.hpp"
#include <sstream>

static std::string intToString(size_t value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

static std::string intToString(int value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}


Response::Response() : statusCode(200),
        statusMessage("OK"), body("") {}

void Response::setStatus(int status) {
    statusCode = status;
    switch (status) {
        case 200:
            statusMessage = "OK";
            break;
        case 400:
            statusMessage = "Bad Request";
            break;
        case 403:
            statusMessage = "Forbidden";
            break;
        case 404:
            statusMessage = "Not Found";
            break;
        case 405:
            statusMessage = "Method Not Allowed";
            break;
        case 408:
            statusMessage = "Request Timeout";
            break;
        case 411:
            statusMessage = "Length Required";
            break;
        case 413:
            statusMessage = "Payload Too Large";
            break;
        case 500:
            statusMessage = "Internal Server Error";
            break;
        case 501:
            statusMessage = "Not Implemented";
            break;
        case 502:
            statusMessage = "Bad Gateway";
            break;
        default:
            statusMessage = "Unknown";
            break;
    }
}

void Response::setHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
}

void Response::setBody(const std::string& content) {
    body = content;
}

std::string Response::build() const {
    std::string result;
    result += "HTTP/1.1 ";
    result += intToString(statusCode);
    result += " ";
    result += statusMessage;
    result += "\r\n";
    std::map<std::string, std::string> tempHeaders = headers;
    tempHeaders["Content-Length"] = intToString(body.size());
    for (std::map<std::string, std::string>::const_iterator it = tempHeaders.begin();
        it != tempHeaders.end(); ++it)
    {
        result += it->first;
        result += ": ";
        result += it->second;
        result += "\r\n";
    }
    result += "\r\n";
    result += body;
    return result;
}