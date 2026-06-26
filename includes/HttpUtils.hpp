#ifndef HTTPUTILS_HPP
#define HTTPUTILS_HPP

#include <string>
#include <vector>

class HttpUtils
{
public:
    static int parseStatusCode(const std::string &status);
    static std::string extractMultipartBoundary(const std::string &contentType);
    static std::string extractFilenameFromHeaders(const std::string &headers);
    static bool extractMultipartFiles(const std::string &body, const std::string &boundary,
                                      std::vector<std::string> &filenames, std::vector<std::string> &contents);
    static std::string getMimeType(const std::string &path);
    static bool isValidMethod(const std::string &method);
    static bool isValidHttpVersion(const std::string &version);
};

#endif
