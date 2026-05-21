#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>

#include <string>
#include <vector>

class Location {
public:
    Location();       
    void reset();
    
    std::string path;
    std::string root;
    std::string index;
    std::string autoindex;
    std::string redirect;
    int         redirect_code;
    std::string alias;

    std::vector<std::string> methods;

    std::vector<std::string> cgi_path;
    std::vector<std::string> cgi_ext;

    bool upload_enable;
    bool upload_enable_set;
    std::string upload_path;
    bool upload_path_set;

    int client_max_body_size;
    bool client_max_body_size_set;
};


#endif