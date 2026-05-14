#ifndef METHODHANDLER_HPP
#define METHODHANDLER_HPP

#include <string>
#include "Request.hpp"
#include "Response.hpp"
#include "Server.hpp"

class MethodHandler
{
public:
    static Response handle(const Request &req, const Server &server);

private:
    static Response handleGet(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc);
    static Response handlePost(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc);
    static Response handleDelete(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc);
};

#endif