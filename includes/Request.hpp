#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>

struct Request
{
    std::string method;
    std::string path;
    std::string version;
    std::string body;
};

#endif