#include "../../includes/FileHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

namespace
{
    const char *DOC_ROOT = "./www";

    bool containsDotDot(const std::string &path)
    {
        size_t i = 0;
        while (i < path.size())
        {
            size_t next = path.find('/', i);
            size_t len = (next == std::string::npos) ? (path.size() - i) : (next - i);
            if (len == 2 && path.compare(i, 2, "..") == 0)
                return true;
            if (next == std::string::npos)
                break;
            i = next + 1;
        }
        return false;
    }

    bool normalizePath(const std::string &rawPath, std::string &normalized)
    {
        std::string path = rawPath;

        size_t q = path.find('?');
        if (q != std::string::npos)
            path.erase(q);

        size_t h = path.find('#');
        if (h != std::string::npos)
            path.erase(h);

        if (path.empty())
            path = "/";

        if (path[0] != '/')
            path = "/" + path;

        if (path.find('\\') != std::string::npos)
            return false;

        if (containsDotDot(path))
            return false;

        normalized = path;
        return true;
    }
}

std::string FileHandler::buildResponse(const std::string &status,
                                       const std::string &body,
                                       const std::string &contentType,
                                       const std::string &connection)
{
    std::ostringstream oss;

    oss << "HTTP/1.1 " << status << "\r\n";
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: " << connection << "\r\n";
    oss << "\r\n";
    oss << body;

    return oss.str();
}

std::string FileHandler::get(const std::string &path, const std::string &connection)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse("403 Forbidden", "Forbidden", "text/plain", connection);

    std::string fullPath = std::string(DOC_ROOT) + normalized;

    std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return buildResponse("404 Not Found", "File not found", "text/plain", connection);

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buildResponse("200 OK", buffer.str(), "text/plain", connection);
}

std::string FileHandler::post(const std::string &path, const std::string &body, const std::string &connection)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse("403 Forbidden", "Forbidden", "text/plain", connection);

    std::string fullPath = std::string(DOC_ROOT) + normalized;

    std::ofstream file(fullPath.c_str(), std::ios::out | std::ios::binary);
    if (!file.is_open())
        return buildResponse("500 Internal Server Error", "Cannot write file", "text/plain", connection);

    file << body;
    file.close();

    return buildResponse("201 Created", "File created/updated", "text/plain", connection);
}

std::string FileHandler::del(const std::string &path, const std::string &connection)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse("403 Forbidden", "Forbidden", "text/plain", connection);

    std::string fullPath = std::string(DOC_ROOT) + normalized;

    if (access(fullPath.c_str(), F_OK) != 0)
        return buildResponse("404 Not Found", "File not found", "text/plain", connection);

    if (std::remove(fullPath.c_str()) != 0)
        return buildResponse("500 Internal Server Error", "Delete failed", "text/plain", connection);

    return buildResponse("200 OK", "File deleted", "text/plain", connection);
}