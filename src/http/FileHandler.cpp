#include "../../includes/FileHandler.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>

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

bool ensureDir(const std::string &path)
{
    size_t pos = 1;
    while ((pos = path.find('/', pos)) != std::string::npos)
    {
        std::string dir = path.substr(0, pos);
        if (access(dir.c_str(), F_OK) != 0)
        {
            if (mkdir(dir.c_str(), 0755) != 0)
                return false;
        }
        pos++;
    }
    return true;
}

bool writeAll(int fd, const std::string &body)
{
    size_t total = 0;
    while (total < body.size())
    {
        ssize_t written = write(fd, body.data() + total, body.size() - total);
        if (written <= 0)
            return false;
        total += (size_t)written;
    }
    return true;
}
bool createTempFile(const std::string &targetPath, std::string &tempPath, int &fd)
{
    std::string dir = targetPath.substr(0, targetPath.find_last_of('/'));

    if (dir.empty())
        dir = ".";

    for (int i = 0; i < 100; i++)
    {
        std::ostringstream oss;
        oss << dir << "/.tmp_" << getpid() << "_" << i;

        tempPath = oss.str();

        fd = open(tempPath.c_str(),
                  O_WRONLY | O_CREAT | O_EXCL,
                  0644);

        if (fd >= 0)
            return true;
    }
    return false;
}

// ================= RESPONSE =================

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

// ================= GET =================

Response FileHandler::get(const std::string &fullPath,
                          const std::string &connection)
{
    std::ifstream file(fullPath.c_str(), std::ios::binary);
    if (!file.is_open())
        return buildResponse(404, "Not Found", "text/plain", connection);

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buildResponse(200, buffer.str(), "text/plain", connection);
}
// ================= POST =================
Response FileHandler::post(const std::string &fullPath,
                           const std::string &body,
                           const std::string &connection)
{
    size_t slash = fullPath.find_last_of('/');
    if (slash == std::string::npos)
        return buildResponse(400, "Bad Request", "text/plain", connection);

    std::string dir = fullPath.substr(0, slash);

    struct stat st;
    if (stat(dir.c_str(), &st) != 0)
    {
        if (mkdir(dir.c_str(), 0755) != 0)
            return buildResponse(500, "Cannot create directory", "text/plain", connection);
    }

    std::string tempPath;
    int fd;

    for (int i = 0; i < 100; i++)
    {
        std::ostringstream oss;
        oss << dir << "/.tmp_" << getpid() << "_" << i;
        tempPath = oss.str();

        fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0)
            break;
    }

    if (fd < 0)
        return buildResponse(500, "Cannot create temp file", "text/plain", connection);

    if (!writeAll(fd, body))
    {
        close(fd);
        std::remove(tempPath.c_str());
        return buildResponse(500, "Write failed", "text/plain", connection);
    }

    close(fd);

    if (std::rename(tempPath.c_str(), fullPath.c_str()) != 0)
    {
        std::remove(tempPath.c_str());
        return buildResponse(500, "Rename failed", "text/plain", connection);
    }

    return buildResponse(201, "Created", "text/plain", connection);
}
// ================= DELETE =================
Response FileHandler::del(const std::string &fullPath,
                          const std::string &connection)
{
    if (access(fullPath.c_str(), F_OK) != 0)
        return buildResponse(404, "Not Found", "text/plain", connection);

    if (std::remove(fullPath.c_str()) != 0)
        return buildResponse(500, "Delete failed", "text/plain", connection);

    return buildResponse(200, "Deleted", "text/plain", connection);
}