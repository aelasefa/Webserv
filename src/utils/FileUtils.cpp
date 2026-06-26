#include "../../includes/FileUtils.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>

std::string FileUtils::readFile(const std::string &path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

bool FileUtils::createTempFile(const std::string &targetPath, std::string &tempPath, int &fd)
{
    std::string dir = targetPath.substr(0, targetPath.find_last_of('/'));
    if (dir.empty())
        dir = ".";

    for (int i = 0; i < 100; i++)
    {
        std::ostringstream oss;
        oss << dir << "/.tmp_" << getpid() << "_" << i;
        tempPath = oss.str();
        fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
        if (fd >= 0)
            return true;
    }
    return false;
}

bool FileUtils::fileExists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool FileUtils::isDirectory(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

std::string FileUtils::getExtension(const std::string &path)
{
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    return path.substr(dot);
}

std::string FileUtils::basename(const std::string &path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    if (pos + 1 >= path.size())
        return "";
    return path.substr(pos + 1);
}

std::string FileUtils::joinPaths(const std::string &left, const std::string &right)
{
    if (left.empty())
        return right.empty() ? "/" : right;
    if (right.empty())
        return left;
    if (left[left.size() - 1] == '/' && right[0] == '/')
        return left + right.substr(1);
    if (left[left.size() - 1] != '/' && right[0] != '/')
        return left + "/" + right;
    return left + right;
}

std::string FileUtils::sanitizeFilename(const std::string &raw)
{
    if (raw.empty())
        return "";

    if (raw.find('\0') != std::string::npos || raw.find('/') != std::string::npos || raw.find("..") != std::string::npos)
        return "";

    size_t start = raw.find_first_not_of(". ");
    if (start == std::string::npos)
        return "";

    std::string safe = raw.substr(start);

    if (safe.empty() || safe.find_first_not_of('.') == std::string::npos)
        return "";

    return safe;
}
