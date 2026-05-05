#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "Config.hpp"


class ConfigParser {
    private:
    
    bool is_server_line(const std::string& line);
    bool is_location_line(const std::string& line);
    bool is_close_brace(const std::string& line);
    void extract_location_path(const std::string& line, Location& loc);
    void parse_location_directive(const std::string& key, std::istringstream& iss, Location& loc);
    void parse_server_directive(const std::string& key, std::istringstream& iss, Server& srv);
    public:

    Config parse(const std::string& filename);
};

#endif