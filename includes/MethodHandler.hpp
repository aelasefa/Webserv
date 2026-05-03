#ifndef METHODHANDLER_HPP
#define METHODHANDLER_HPP

#include <string>
#include "Request.hpp"

class MethodHandler
{
public:
    static std::string handle(const Request &req);

private:
    static std::string handleGet(const Request &req);
    static std::string handlePost(const Request &req);
    static std::string handleDelete(const Request &req);
};

#endif