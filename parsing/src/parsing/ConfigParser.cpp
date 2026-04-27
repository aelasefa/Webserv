#include "../../include/config/ConfigParser.hpp"


static std::string trim(const std::string& s)
{
    size_t start = 0;
    size_t end = s.length();

    while (start < end && (s[start] == ' ' || s[start] == '\t'))
        start++;

    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n'))
        end--;

    return s.substr(start, end - start);
}

Config ConfigParser::parse(const std::string& filename)
{
    Config config;
    // std::ifstream file(filename);
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cout << "Failed to open file\n";
        return config;
    }
    std::string line;
    Server current_server;
    Location current_location;

    State state = NONE;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line.find("server") != std::string::npos && line.find("{") != std::string::npos)
        {
            state = SERVER;
            continue;
        }
        if (line.find("location") != std::string::npos && line.find("{") != std::string::npos)
        {
            state = LOCATION;

            std::istringstream iss(line);
            std::string tmp;
            iss >> tmp >> current_location.path;

            continue;
        }

        if (line == "}")
        {
            if (state == LOCATION)
            {
                current_server.locations.push_back(current_location);
                current_location = Location();
                state = SERVER;
                continue;
            }

            if (state == SERVER)
            {
                config.servers.push_back(current_server);
                current_server = Server();
                state = NONE;
                continue;
            }
        }

        if (state == LOCATION)
        {
            std::string key;
            std::istringstream iss(line);

            iss >> key;

            if (key == "allow_methods")
            {
                std::string v;
                while (iss >> v)
                    current_location.methods.push_back(trim(v));
            }
            else
            {
                std::string value;
                iss >> value;
                value = trim(value);
                if (key == "root") current_location.root = value;
                else if (key == "index") current_location.index = value;
                else if (key == "autoindex") current_location.autoindex = value;
                else if (key == "return") current_location.redirect = value;
                else if (key == "alias") current_location.alias = value;
                else if (key == "cgi_path") current_location.cgi_path.push_back(value);
                else if (key == "cgi_ext") current_location.cgi_ext.push_back(value);
            }
        }

        else if (state == SERVER)
        {
            std::string key, value;
            std::istringstream iss(line);

            iss >> key >> value;
            value = trim(value);
            if (key == "listen") current_server.listen = std::atoi(value.c_str());
            else if (key == "host") current_server.host = value;
            else if (key == "root") current_server.root = value;
            else if (key == "server_name") current_server.server_name = value;
            else if (key == "index") current_server.index = value;
            else if (key == "client_max_body_size")
                current_server.client_max_body_size = std::atoi(value.c_str());
            else if (key == "error_page")
            {
                int code = std::atoi(value.c_str());
                std::string path;
                if (iss >> path)
                    current_server.error_pages[code] = path;
            }
        }
    }
    if (state == SERVER)
         config.servers.push_back(current_server);
    return config;
}

