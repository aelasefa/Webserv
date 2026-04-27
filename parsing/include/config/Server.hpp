#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include "Location.hpp"

#include <map>

class Server {
public:
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