#ifndef METHODHANDLER_HPP
#define METHODHANDLER_HPP

#include <string>
#include "Request.hpp"
#include "Response.hpp"

class MethodHandler
{
public:
    static Response handle(const Request &req);

private:
    static Response handleGet(const Request &req);
    static Response handlePost(const Request &req);
    static Response handleDelete(const Request &req);
};

#endif