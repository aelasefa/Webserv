
#include "../includes/Router.hpp"
#include "../includes/Utils.hpp"
#include "../includes/CGI.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <cstdlib>

Response Router::serveError(int code)
{
    Response response;
    response.setStatus(code);
    response.setHeader("Content-Type", "text/html");
    std::string body = "<html><body><h1>" + Utils::toString(code) + " Error</h1></body></html>";
    response.setBody(body);
    response.setHeader("Connection", "close");
    return response;
}

Response Router::serveRedirect(int code, const std::string& location)
{
    Response response;
    response.setStatus(code);
    response.setHeader("Location", location);
    response.setHeader("Connection", "close");
    return response;
}

Response Router::serveStaticFile(const std::string& path)
{
    Response response;
    std::string content = readFile(path);
    if (content.empty() && !fileExists(path))
    {
        return serveError(404);
    }
    response.setStatus(200);
    response.setHeader("Content-Type", getMimeType(path));
    response.setBody(content);
    return response;
}

Response Router::serveCgi(Server& server, Location& location, Request& request, const std::string& script_path)
{
    Response response;
    std::string ext = getExtension(script_path);
    std::string interpreter = findInterpreter(ext, location.cgi_ext, location.cgi_path);
    if (interpreter.empty())
    {
        return serveError(500);
    }
    CGI cgi;
    cgi.setScriptPath(script_path);
    cgi.setInterpreter(interpreter);
    std::string cgi_output = cgi.execute(request);
    if (cgi_output.empty())
    {
        return serveError(502);
    }
    size_t header_end = cgi_output.find("\r\n\r\n");
    std::string cgi_body;
    std::string content_type = "text/html";
    int status_code = 200;
    if (header_end != std::string::npos)
    {
        std::string headers_part = cgi_output.substr(0, header_end);
        cgi_body = cgi_output.substr(header_end + 4);
        std::istringstream iss(headers_part);
        std::string line;
        while (std::getline(iss, line))
        {
            if (line.empty() || line == "\r")
                continue;
            if (line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            size_t sep = line.find(':');
            if (sep != std::string::npos)
            {
                std::string key = Utils::toLower(Utils::trim(line.substr(0, sep)));
                std::string value = Utils::trim(line.substr(sep + 1));
                if (key == "content-type")
                    content_type = value;
                else if (key == "status")
                    status_code = std::atoi(value.c_str());
            }
        }
    }
    else
    {
        cgi_body = cgi_output;
    }
    response.setStatus(status_code);
    response.setHeader("Content-Type", content_type);
    response.setBody(cgi_body);
    return response;
}

std::string Router::buildPath(Server& server, Location& location, const std::string& request_path)
{
    std::string root = location.root.empty() ? server.root : location.root;
    std::string cleaned_path = request_path;
    size_t q = cleaned_path.find('?');
    if (q != std::string::npos)
        cleaned_path = cleaned_path.substr(0, q);
    if (!root.empty() && root[root.size() - 1] == '/' && !cleaned_path.empty() && cleaned_path[0] == '/')
        return root + cleaned_path.substr(1);
    if (!root.empty() && root[root.size() - 1] != '/' && (!cleaned_path.empty() && cleaned_path[0] != '/'))
        return root + "/" + cleaned_path;
    return root + cleaned_path;
}

bool Router::fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool Router::isDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

std::string Router::getExtension(const std::string& path)
{
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "";
    return path.substr(dot);
}

std::string Router::getMimeType(const std::string& path)
{
    std::string ext = getExtension(path);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".txt") return "text/plain";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".zip") return "application/zip";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".mp3") return "audio/mpeg";
    return "application/octet-stream";
}

bool Router::isMethodAllowed(const std::string& method, const std::vector<std::string>& allowed)
{
    if (allowed.empty())
        return true;
    for (size_t i = 0; i < allowed.size(); i++)
    {
        if (allowed[i] == method)
            return true;
    }
    return false;
}

bool Router::isCgiExtension(const std::string& ext, const std::vector<std::string>& cgi_ext)
{
    for (size_t i = 0; i < cgi_ext.size(); i++)
    {
        if (cgi_ext[i] == ext)
            return true;
    }
    return false;
}

std::string Router::findInterpreter(const std::string& ext, const std::vector<std::string>& cgi_ext, const std::vector<std::string>& cgi_path)
{
    for (size_t i = 0; i < cgi_ext.size() && i < cgi_path.size(); i++)
    {
        if (cgi_ext[i] == ext)
            return cgi_path[i];
    }
    return "";
}

std::string Router::readFile(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return "";
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

std::string Router::generateDirectoryListing(const std::string& full_path, const std::string& request_path)
{
    DIR* dir = opendir(full_path.c_str());
    if (!dir)
        return "";
    std::string html = "<html><head><title>Index of " + request_path + "</title></head><body>\n";
    html += "<h1>Index of " + request_path + "</h1><hr><pre>\n";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        std::string link = request_path;
        if (!link.empty() && link[link.size() - 1] != '/')
            link += "/";
        link += name;
        html += "<a href=\"" + link + "\">" + name + "</a>\n";
    }
    closedir(dir);
    html += "</pre><hr></body></html>";
    return html;
}

Response Router::routeRequest(Server& server, Request& request)
{
    Location* matched_location = NULL;
    size_t best_match = 0;
    std::string request_path = request.getPath();
    size_t q = request_path.find('?');
    if (q != std::string::npos)
        request_path = request_path.substr(0, q);
    for (size_t i = 0; i < server.locations.size(); i++)
    {
        Location& loc = server.locations[i];
        std::string loc_path = loc.path;
        bool is_exact = false;
        bool is_prefix = true;
        if (!loc_path.empty() && loc_path[0] == '=')
        {
            is_exact = true;
            is_prefix = false;
            loc_path = Utils::trim(loc_path.substr(1));
        }
        if (is_exact)
        {
            if (request_path == loc_path && loc_path.size() > best_match)
            {
                matched_location = &loc;
                best_match = loc_path.size();
            }
        }
        else if (is_prefix)
        {
            if (request_path.find(loc_path) == 0 && loc_path.size() > best_match)
            {
                matched_location = &loc;
                best_match = loc_path.size();
            }
        }
    }
    if (!matched_location)
    {
        return serveError(404);
    }
    Location& location = *matched_location;
    if (!isMethodAllowed(request.getMethod(), location.methods))
    {
        return serveError(405);
    }
    if (!location.redirect.empty())
    {
        return serveRedirect(location.redirect_code, location.redirect);
    }
    std::string full_path = buildPath(server, location, request_path);
    if (!fileExists(full_path))
    {
        return serveError(404);
    }
    if (isDirectory(full_path))
    {
        if (!location.index.empty())
        {
            std::string index_path = full_path;
            if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
                index_path += "/";
            index_path += location.index;
            if (fileExists(index_path))
            {
                full_path = index_path;
            }
            else
            {
                if (location.autoindex == "on")
                {
                    Response response;
                    response.setStatus(200);
                    response.setHeader("Content-Type", "text/html");
                    response.setBody(generateDirectoryListing(full_path, request_path));
                    return response;
                }
                return serveError(403);
            }
        }
        else
        {
            if (location.autoindex == "on")
            {
                Response response;
                response.setStatus(200);
                response.setHeader("Content-Type", "text/html");
                response.setBody(generateDirectoryListing(full_path, request_path));
                return response;
            }
            return serveError(403);
        }
    }
    std::string ext = getExtension(full_path);
    if (isCgiExtension(ext, location.cgi_ext))
    {
        return serveCgi(server, location, request, full_path);
    }
    return serveStaticFile(full_path);
}