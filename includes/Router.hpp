#ifndef ROUTER_HPP
#define ROUTER_HPP

#include "StringUtils.hpp"
#include "CGI.hpp"
#include "FileHandler.hpp"
#include "MimeTypes.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "Utils.hpp"
#include "Server.hpp"
#include "Location.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <string>
#include <vector>

class Router
{
public:
    static Response routeRequest(const Server& server, Request& request);

private:
    static std::string buildPath(const Server& server, const Location& location, const std::string& request_path);
    static bool fileExists(const std::string& path);
    static bool isDirectory(const std::string& path);
    static std::string getExtension(const std::string& path);
    static std::string getMimeType(const std::string& path);
    static bool isMethodAllowed(const std::string& method, const std::vector<std::string>& allowed);
    static bool isCgiExtension(const std::string& ext, const std::vector<std::string>& cgi_ext);
    static std::string findInterpreter(const std::string& ext, const std::vector<std::string>& cgi_ext, const std::vector<std::string>& cgi_path);
    static std::string generateDirectoryListing(const std::string& full_path, const std::string& request_path);
    static Response serveStaticFile(const std::string& path);
    static Response serveRedirect(int code, const std::string& location);
    static Response serveError(int code);
    static Response serveCgi(const Location& location, const Request& request, const std::string& script_path);
};

#endif