
#include "../../includes/ConfigParser.hpp"
#include "../../includes/Utils.hpp"
#include <cerrno>
#include <cstdlib>
#include <cctype>

static unsigned long parse_unsigned_long(const std::string &str, bool &ok)
{
    ok = false;
    if (str.empty())
        return 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (!std::isdigit(str[i]))
            return 0;
    }
    errno = 0;
    unsigned long val = std::strtoul(str.c_str(), NULL, 10);
    if (errno == ERANGE)
        return 0;
    ok = true;
    return val;
}

Location::Location() {
    path = "";
    root = "";
    index = "";
    autoindex = "";
    redirect = "";
    redirect_code = 301;
    alias = "";
    upload_enable = false;
    upload_enable_set = false;
    upload_path = "";
    upload_path_set = false;
    client_max_body_size = 0;
    client_max_body_size_set = false;
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
    upload_enable = false;
    upload_enable_set = false;
    upload_path = "";
    upload_path_set = false;
    client_max_body_size = 0;
    client_max_body_size_set = false;
}

Server::Server() {
    listen = 8080;                      
    root = "";
    host = "";
    server_name = "";
    index = "";
    client_max_body_size = 1048576;     
    upload_enable = false;
    upload_path = "";
}

void Server::reset() {
    listen = 8080;
    host = "";
    server_name = "";
    root = "";
    index = "";
    client_max_body_size = 1048576;
    upload_enable = false;
    upload_path = "";
    error_pages.clear();
    locations.clear();
}


bool ConfigParser::is_server_line(const std::string& line)
{
    std::string trimmed = StringUtils::trim(line);

    if (trimmed.compare(0, 6, "server") != 0)
        return false;

    if (trimmed.size() > 6 &&
        !std::isspace(trimmed[6]) &&
        trimmed[6] != '{')
        return false;

    return trimmed.find('{') != std::string::npos;
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
    else if (key == "upload_enable") {
        std::string value;
        iss >> value;
        value = strip_semicolon(value);
        loc.upload_enable_set = true;
        loc.upload_enable = (value == "on");
    }
    else if (key == "upload_path" || key == "upload_store") {
        std::string value;
        iss >> value;
        loc.upload_path = strip_semicolon(value);
        loc.upload_path_set = true;
    }
    else if (key == "client_max_body_size") {
        std::string value;
        iss >> value;
        value = strip_semicolon(value);
        bool ok;
        unsigned long val = parse_unsigned_long(value, ok);
        if (ok && val <= 2147483647UL)
        {
            loc.client_max_body_size = static_cast<int>(val);
            loc.client_max_body_size_set = true;
        }
        else
        {
            loc.client_max_body_size = 1000000;
            loc.client_max_body_size_set = true;
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
        bool ok;
        unsigned long val = parse_unsigned_long(value, ok);
        if (ok && val >= 1 && val <= 65535)
        {
            srv.listen = static_cast<int>(val);
        }
        else
        {
            throw std::runtime_error("invalid listen port: " + value);
        }
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
        bool ok;
        unsigned long val = parse_unsigned_long(value, ok);
        if (ok && val <= 2147483647UL)
            srv.client_max_body_size = static_cast<int>(val);
        else
            srv.client_max_body_size = 1000000;
    }
    else if (key == "upload_enable") {
        std::string value;
        iss >> value;
        value = strip_semicolon(value);
        srv.upload_enable = (value == "on");
    }
    else if (key == "upload_path" || key == "upload_store") {
        std::string value;
        iss >> value;
        srv.upload_path = strip_semicolon(value);
    }
    else if (key == "error_page") {
        std::vector<int> codes;
        std::string token;

        while (iss >> token) {
            bool ok;
            unsigned long val = parse_unsigned_long(token, ok);
            if (ok && val >= 100 && val <= 599) {
                codes.push_back(static_cast<int>(val));
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
                if (!(current_server.listen == 0 && current_server.server_name.empty() && current_server.locations.empty())) {
                    config.servers.push_back(current_server);
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