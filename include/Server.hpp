#ifndef SERVER_HPP
#define SERVER_HPP


#include "Location.hpp"

#include <map>

class Server {
public:
    void reset();
    Server();
    int listen;
    std::string host;
    std::string root;
    std::string server_name;
    std::string index;

    int client_max_body_size;

    std::map<int, std::string> error_pages;

    std::vector<Location> locations;
};
#endif