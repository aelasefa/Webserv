#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <iostream>

class request{
    public:
        std::string method;
        std::string version;
        std::string path;
        std::string body;
};




#endif