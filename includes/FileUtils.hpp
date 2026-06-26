#ifndef FILEUTILS_HPP
#define FILEUTILS_HPP

#include <string>

class FileUtils
{
public:
    static std::string readFile(const std::string &path);
    static bool createTempFile(const std::string &targetPath, std::string &tempPath, int &fd);
    static bool fileExists(const std::string &path);
    static bool isDirectory(const std::string &path);
    static std::string getExtension(const std::string &path);
    static std::string basename(const std::string &path);
    static std::string joinPaths(const std::string &left, const std::string &right);
    static std::string sanitizeFilename(const std::string &raw);
};

#endif
