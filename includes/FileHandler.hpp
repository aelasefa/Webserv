#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>
#include "Response.hpp"

class FileHandler
{
public:
    static bool normalizePath(const std::string &rawPath, std::string &normalized);
    static bool ensureDir(const std::string &path);
    static Response get(const std::string& fullPath, const std::string& connection);
    static Response post(const std::string& fullPath, const std::string& body, const std::string& connection);
    static Response del(const std::string& fullPath, const std::string& connection);

private:
    static Response buildResponse(int status,
                                  const std::string &body,
                                  const std::string &contentType,
                                  const std::string &connection);
};

#endif