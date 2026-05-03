
#include "../../include/config/ConfigParser.hpp"

Location::Location() {
    path = "";
    root = "";
    index = "";
    autoindex = "";
    redirect = "";
    redirect_code = 301;
    alias = "";
}

static std::string trim(const std::string& s) {
    size_t start = 0;
    size_t end = s.length();

    while (start < end && (s[start] == ' ' || s[start] == '\t'))
        start++;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' || s[end - 1] == '\r'))
        end--;

    return s.substr(start, end - start);
}

static std::string strip_semicolon(const std::string& s) {
    std::string result = trim(s);
    if (!result.empty() && result[result.length() - 1] == ';')
        result.erase(result.length() - 1);
    return result;
}

static std::string remove_cr(const std::string& s) {
    std::string result = s;
    if (!result.empty() && result[result.length() - 1] == '\r')
        result.erase(result.length() - 1);
    return result;
}

static bool is_number(const std::string& s) {
    if (s.empty()) return false;
    for (size_t i = 0; i < s.length(); i++)
        if (!isdigit(s[i])) return false;
    return true;
}

void Location::reset() {
    path = "";
    root = "";
    index = "";
    autoindex = "";
    redirect = "";
    redirect_code = 301;
    alias = "";
    methods.clear();
    cgi_path.clear();
    cgi_ext.clear();
}

Server::Server() {
    listen = 8080;                      
    root = "";
    host = "";
    server_name = "";
    index = "";
    client_max_body_size = 1048576;     
}

void Server::reset() {
    listen = 8080;
    host = "";
    server_name = "";
    root = "";
    index = "";
    client_max_body_size = 1048576;
    error_pages.clear();
    locations.clear();
}


bool ConfigParser::is_server_line(const std::string& line) {
    return (line.find("server") != std::string::npos && 
            line.find("{") != std::string::npos);
}

bool ConfigParser::is_location_line(const std::string& line) {
    return (line.find("location") != std::string::npos && 
            line.find("{") != std::string::npos);
}

bool ConfigParser::is_close_brace(const std::string& line) {
    return (trim(line) == "}");
}


void ConfigParser::extract_location_path(const std::string& line, Location& loc) {
    std::istringstream iss(line);
    std::string token;
    std::string modifier;
    std::string path;

    iss >> token;  

    iss >> token;   
    if (token == "{" || token.empty()) {
        loc.path = "/";
        return;
    }

    if (token == "=" || token == "~" || token == "~*" || token == "^~") {
        modifier = token;
        iss >> path;
    } else {
        path = token;
        modifier = "";
    }

    if (!path.empty() && path[path.length() - 1] == '{') {
        path.erase(path.length() - 1);
    }
    if (path.empty()) {
        path = "/";
    }

    if (!modifier.empty()) {
        loc.path = modifier + " " + path;
    } else {
        loc.path = path;
    }
}
void ConfigParser::parse_location_directive(const std::string& key, 
                                            std::istringstream& iss, 
                                            Location& loc) {
    if (key == "allow_methods") {
        std::string method;
        while (iss >> method) {
            method = strip_semicolon(method);
            if (!method.empty())
                loc.methods.push_back(method);
        }
    }
    else if (key == "root") {
        std::string value;
        iss >> value;
        loc.root = strip_semicolon(value);
    }
    else if (key == "index") {
        std::string value;
        iss >> value;
        loc.index = strip_semicolon(value);
    }
    else if (key == "autoindex") {
        std::string value;
        iss >> value;
        loc.autoindex = strip_semicolon(value);
    }
   else if (key == "return") {
    std::string first;
    iss >> first;
    std::string second;
    
    if (is_number(first)) {
        loc.redirect_code = std::atoi(first.c_str()); 
        if (iss >> second) {
            loc.redirect = strip_semicolon(second);   
        } else {
            loc.redirect = "/";
            std::cerr << "Warning: 'return " << first << "' with no URL, using '/'" << std::endl;
        }
    } else {
        loc.redirect_code = 301;                     
        loc.redirect = strip_semicolon(first);       
    }
}
    else if (key == "alias") {
        std::string value;
        iss >> value;
        loc.alias = strip_semicolon(value);
    }
    else if (key == "cgi_path") {
        std::string path;
        while (iss >> path) {
            path = strip_semicolon(path);
            if (!path.empty())
                loc.cgi_path.push_back(path);
        }
    }
    else if (key == "cgi_ext") {
        std::string ext;
        while (iss >> ext) {
            ext = strip_semicolon(ext);
            if (!ext.empty())
                loc.cgi_ext.push_back(ext);
        }
    }
}
void ConfigParser::parse_server_directive(const std::string& key, 
                                          std::istringstream& iss, 
                                          Server& srv) {
    if (key == "listen") {
        std::string value;
        iss >> value;
        value = strip_semicolon(value);
        if (!value.empty())
            srv.listen = std::atoi(value.c_str());
    }
    else if (key == "host") {
        std::string value;
        iss >> value;
        srv.host = strip_semicolon(value);
    }
    else if (key == "server_name") {
        std::string value;
        iss >> value;
        srv.server_name = strip_semicolon(value);
    }
    else if (key == "root") {
        std::string value;
        iss >> value;
        srv.root = strip_semicolon(value);
    }
    else if (key == "index") {
        std::string value;
        iss >> value;
        srv.index = strip_semicolon(value);
    }
    else if (key == "client_max_body_size") {
        std::string value;
        iss >> value;
        value = strip_semicolon(value);
        if (!value.empty())
            srv.client_max_body_size = std::atoi(value.c_str());
    }
    else if (key == "error_page") {
        std::vector<int> codes;
        std::string token;

        while (iss >> token) {
            if (is_number(token)) {
                codes.push_back(std::atoi(token.c_str()));
            } else {
                std::string path = strip_semicolon(token);
                for (size_t i = 0; i < codes.size(); i++) {
                    srv.error_pages[codes[i]] = path;
                }
                return;
            }
        }
    }
}

Config ConfigParser::parse(const std::string& filename) {
    Config config;
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cout << "Failed to open file: " << filename << std::endl;
        return config;
    }

    std::string line;
    Server current_server;
    Location current_location;
    State state = NONE;
    int line_number = 0;

    bool has_current_location = false;

    while (std::getline(file, line)) {
        line_number++;
        line = remove_cr(line);

        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed[0] == '#') {
            continue;
        }


        size_t comment_pos = trimmed.find('#');
        if (comment_pos != std::string::npos) {
            trimmed = trimmed.substr(0, comment_pos);
            trimmed = trim(trimmed);
            if (trimmed.empty()) continue;
        }

        if (is_server_line(trimmed)) {
            if (state == LOCATION && has_current_location) {
                current_server.locations.push_back(current_location);
                current_location.reset();
                has_current_location = false;
            }
            if (state == SERVER || state == LOCATION) {
                if (state == LOCATION) {
                }
                if (has_current_location == false) {
                    if (!(current_server.listen == 0 && current_server.server_name.empty() && current_server.locations.empty())) {
                    }
                }
            }
            current_server.reset();
            state = SERVER;
            has_current_location = false;
            continue;
        }

        if (is_location_line(trimmed)) {
            if (state == LOCATION && has_current_location) {
                current_server.locations.push_back(current_location);
                current_location.reset();
            }
            current_location.reset();
            extract_location_path(trimmed, current_location);
            state = LOCATION;
            has_current_location = true;
            continue;
        }

        if (is_close_brace(trimmed)) {
            if (state == LOCATION && has_current_location) {
                current_server.locations.push_back(current_location);
                current_location.reset();
                has_current_location = false;
                state = SERVER;
                continue;
            }
            if (state == SERVER) {
                config.servers.push_back(current_server);
                current_server.reset();
                state = NONE;
                continue;
            }
            continue;
        }

        std::istringstream iss(trimmed);
        std::string key;
        iss >> key;

        if (key.empty()) continue;

        if (state == LOCATION && has_current_location) {
            parse_location_directive(key, iss, current_location);
        }
        else if (state == SERVER) {
            parse_server_directive(key, iss, current_server);
        }
        else if (state == NONE) {

        }
    }

    if (state == LOCATION && has_current_location) {
        current_server.locations.push_back(current_location);
        state = SERVER;
    }
    if (state == SERVER) {
        config.servers.push_back(current_server);
    }

    file.close();
    return config;
}