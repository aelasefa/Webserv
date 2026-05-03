#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>

class FileHandler
{
public:
    static std::string get(const std::string &path, const std::string &connection);
    static std::string post(const std::string &path, const std::string &body, const std::string &connection);
    static std::string del(const std::string &path, const std::string &connection);

private:
    static std::string buildResponse(const std::string &status,
                                     const std::string &body,
                                     const std::string &contentType,
                                     const std::string &connection);
};

#endif