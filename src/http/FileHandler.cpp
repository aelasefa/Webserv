#include "../../includes/FileHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>

namespace
{
    const char *DEFAULT_DOC_ROOT = "./website";

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
    std::string resolveDocRoot(const std::string &docRoot)
    {
        if (docRoot.empty())
            return DEFAULT_DOC_ROOT;
        return docRoot;
    }

    bool writeAll(int fd, const std::string &body)
    {
        size_t total = 0;
        while (total < body.size())
        {
            ssize_t written = write(fd, body.data() + total, body.size() - total);
            if (written <= 0)
                return false;
            total += static_cast<size_t>(written);
        }
        return true;
    }

    bool createTempFile(const std::string &targetPath, std::string &tempPath, int &fd)
    {
        for (int i = 0; i < 100; i++)
        {
            std::ostringstream oss;
            oss << targetPath << ".tmp." << getpid() << "." << i;
            tempPath = oss.str();
            fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (fd >= 0)
                return true;
        }
        return false;
    }
}

Response FileHandler::buildResponse(int status,
                                    const std::string &body,
                                    const std::string &contentType,
                                    const std::string &connection)
{
    Response resp;
    resp.setStatus(status);
    resp.setHeader("Content-Type", contentType);
    resp.setHeader("Connection", connection);
    resp.setBody(body);
    return resp;
}

Response FileHandler::get(const std::string &path, const std::string &connection, const std::string &docRoot)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse(403, "Forbidden", "text/plain", connection);

    if (!normalized.empty() && normalized[normalized.size() - 1] == '/')
        normalized += "index.html";

    const std::string root = resolveDocRoot(docRoot);
    std::string fullPath = root + normalized;

    std::ifstream file(fullPath.c_str(), std::ios::in | std::ios::binary);
    if (!file.is_open())
        return buildResponse(404, "File not found", "text/plain", connection);

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buildResponse(200, buffer.str(), "text/html", connection);
}

Response FileHandler::post(const std::string &path, const std::string &body, const std::string &connection, const std::string &docRoot)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse(403, "Forbidden", "text/plain", connection);

    const std::string root = resolveDocRoot(docRoot);
    std::string fullPath = root + normalized;

    if (access(fullPath.c_str(), F_OK) == 0)
        return buildResponse(403, "File exists", "text/plain", connection);

    std::string tempPath;
    int tempFd = -1;
    if (!createTempFile(fullPath, tempPath, tempFd))
        return buildResponse(500, "Cannot create temp file", "text/plain", connection);

    bool wrote = writeAll(tempFd, body);
    close(tempFd);

    if (!wrote)
    {
        std::remove(tempPath.c_str());
        return buildResponse(500, "Cannot write file", "text/plain", connection);
    }

    if (std::rename(tempPath.c_str(), fullPath.c_str()) != 0)
    {
        std::remove(tempPath.c_str());
        return buildResponse(500, "Cannot finalize file", "text/plain", connection);
    }

    return buildResponse(201, "File created", "text/plain", connection);
}

Response FileHandler::del(const std::string &path, const std::string &connection, const std::string &docRoot)
{
    std::string normalized;
    if (!normalizePath(path, normalized))
        return buildResponse(403, "Forbidden", "text/plain", connection);

    const std::string root = resolveDocRoot(docRoot);
    std::string fullPath = root + normalized;

    if (access(fullPath.c_str(), F_OK) != 0)
        return buildResponse(404, "File not found", "text/plain", connection);

    if (std::remove(fullPath.c_str()) != 0)
        return buildResponse(500, "Delete failed", "text/plain", connection);

    return buildResponse(200, "File deleted", "text/plain", connection);
}