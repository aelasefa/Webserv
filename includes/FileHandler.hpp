#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>
#include "Response.hpp"

class FileHandler
{
public:
    static Response get(const std::string &path, const std::string &connection);
    static Response post(const std::string &path, const std::string &body, const std::string &connection);
    static Response del(const std::string &path, const std::string &connection);

private:
    static Response buildResponse(int status,
                                  const std::string &body,
                                  const std::string &contentType,
                                  const std::string &connection);
};

#endif