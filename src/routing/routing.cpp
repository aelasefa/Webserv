#include "../../includes/Router.hpp"
#include "../../includes/Utils.hpp"
#include "../../includes/CGI.hpp"
#include "../../includes/FileHandler.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace
{
    std::string stripQueryAndFragment(const std::string &path)
    {
        std::string cleaned = path;
        size_t q = cleaned.find('?');
        if (q != std::string::npos)
            cleaned.erase(q);
        size_t h = cleaned.find('#');
        if (h != std::string::npos)
            cleaned.erase(h);
        if (cleaned.empty())
            cleaned = "/";
        return cleaned;
    }

    std::string baseName(const std::string &path)
    {
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos)
            return path;
        if (pos + 1 >= path.size())
            return "";
        return path.substr(pos + 1);
    }

    std::string extractBoundary(const std::string &contentType)
    {
        std::string lower = Utils::toLower(contentType);
        size_t formPos = lower.find("multipart/form-data");
        if (formPos == std::string::npos)
            return "";

        size_t boundaryPos = lower.find("boundary=", formPos);
        if (boundaryPos == std::string::npos)
            return "";

        std::string boundary = contentType.substr(boundaryPos + 9);
        size_t semi = boundary.find(';');
        if (semi != std::string::npos)
            boundary = boundary.substr(0, semi);
        boundary = Utils::trim(boundary);
        if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
            boundary = boundary.substr(1, boundary.size() - 2);
        return boundary;
    }
    std::string sanitizeFilename(const std::string &raw)
    {
        if (raw.empty())
            return "";

        if (raw.find('\0') != std::string::npos || raw.find('/') != std::string::npos || raw.find("..") != std::string::npos)
            return "";

        size_t start = raw.find_first_not_of(". ");
        if (start == std::string::npos)
            return "";

        std::string safe = raw.substr(start);

        if (safe.empty() || safe.find_first_not_of('.') == std::string::npos)
            return "";

        return safe;
    }

    std::string extractFilenameFromHeaders(const std::string &headers)
    {
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);

            size_t sep = line.find(':');
            if (sep == std::string::npos)
                continue;

            std::string key = Utils::toLower(Utils::trim(line.substr(0, sep)));
            std::string value = Utils::trim(line.substr(sep + 1));

            if (key != "content-disposition")
                continue;

            std::string lowerValue = Utils::toLower(value);
            size_t fnPos = lowerValue.find("filename=");
            if (fnPos == std::string::npos)
                return "";

            std::string filename = value.substr(fnPos + 9);
            size_t semi = filename.find(';');
            if (semi != std::string::npos)
                filename = filename.substr(0, semi);
            filename = Utils::trim(filename);
            if (filename.size() >= 2 && filename[0] == '"' && filename[filename.size() - 1] == '"')
                filename = filename.substr(1, filename.size() - 2);

            return filename;
        }

        return "";
    }

    bool extractMultipartFiles(const std::string &body, const std::string &boundary,
                               std::vector<std::string> &filenames,
                               std::vector<std::string> &contents)
    {
        std::string marker = "--" + boundary;
        size_t pos = body.find(marker);
        if (pos == std::string::npos)
            return false;

        pos += marker.size();
        if (body.compare(pos, 2, "--") == 0)
            return true;
        if (body.compare(pos, 2, "\r\n") != 0)
            return false;
        pos += 2;

        while (true)
        {
            size_t headersEnd = body.find("\r\n\r\n", pos);
            if (headersEnd == std::string::npos)
                return false;

            std::string headers = body.substr(pos, headersEnd - pos);
            std::string filename = extractFilenameFromHeaders(headers);
            pos = headersEnd + 4;

            size_t nextBoundary = body.find("\r\n" + marker, pos);
            if (nextBoundary == std::string::npos)
                return false;

            std::string partData = body.substr(pos, nextBoundary - pos);
            if (!filename.empty())
            {
                filenames.push_back(filename);
                contents.push_back(partData);
            }

            pos = nextBoundary + 2 + marker.size();
            if (body.compare(pos, 2, "--") == 0)
                break;
            if (body.compare(pos, 2, "\r\n") != 0)
                return false;
            pos += 2;
        }

        return true;
    }
}

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

    if (!fileExists(path))
        return serveError(404);

    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode))
        return serveError(404);

    response.setStatus(200);
    response.setHeader("Content-Type", getMimeType(path));
    // Don't read the file here: just point the response at it. The actual
    // bytes are read and sent in small chunks from Client::sendData(),
    // driven by the poll() loop, so one large file can't block the whole
    // server while it's being read off disk.
    response.setFileBody(path, static_cast<size_t>(st.st_size));
    return response;
}

Response Router::serveCgi(const Location& location, const Request& request, const std::string& script_path)
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

std::string Router::buildPath(const Server& server,const Location& location,const std::string& request_path)
{
    std::string root = location.root.empty() ? server.root : location.root;
    std::string path = request_path;

    size_t q = path.find('?');
    if (q != std::string::npos)
        path = path.substr(0, q);

    if (!location.root.empty() &&
        location.path != "/" &&
        path.compare(0, location.path.size(), location.path) == 0)
    {
        path.erase(0, location.path.size());

        if (path.empty() || path[0] != '/')
            path = "/" + path;
    }
    if (!root.empty() && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    if (!path.empty() && path[0] != '/')
        path = "/" + path;

    return root + path;
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

Response Router::routeRequest(const Server& server, Request& request)
{
    const Location* matched_location = NULL;
    size_t best_match = 0;
    std::string request_path = stripQueryAndFragment(request.getPath());
    for (size_t i = 0; i < server.locations.size(); i++)
    {
        
        const Location& loc = server.locations[i];
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
    const Location *location = matched_location;
    if (!location)
        return serveError(404);
    int effectiveBodyLimit = server.client_max_body_size;
    if (location && location->client_max_body_size_set)
        effectiveBodyLimit = location->client_max_body_size;
    if (effectiveBodyLimit > 0 && request.getBody().size() > static_cast<size_t>(effectiveBodyLimit))
        return serveError(413);

    if (location && !isMethodAllowed(request.getMethod(), location->methods))
        return serveError(405);

    if (location && !location->redirect.empty())
        return serveRedirect(location->redirect_code, location->redirect);

    if (request.getMethod() == "POST")
    {
        bool uploadEnabled = server.upload_enable;
        std::string uploadPath = server.upload_path;

        if (location && location->upload_enable_set)
            uploadEnabled = location->upload_enable;

        if (location && location->upload_path_set)
            uploadPath = location->upload_path;

        if (!uploadEnabled)
            return serveError(403);

        if (location)
        {
            std::string full_path = buildPath(server, *location, request_path);
            std::string ext = getExtension(full_path);

            if (isCgiExtension(ext, location->cgi_ext))
            {
                if (!fileExists(full_path))
                    return serveError(404);
                return serveCgi(*location, request, full_path);
            }
        }

        if (uploadPath.empty())
            return serveError(403);

        if (!FileHandler::ensureDir(uploadPath))
            return serveError(500);

        std::string contentType = request.getHeader("Content-Type");
        std::string boundary = extractBoundary(contentType);

        if (!boundary.empty())
        {
            std::vector<std::string> filenames;
            std::vector<std::string> contents;

            if (!extractMultipartFiles(request.getBody(), boundary, filenames, contents))
                return serveError(400);

            if (filenames.empty())
                return serveError(400);

            for (size_t i = 0; i < filenames.size(); i++)
            {
                std::string cleaned = sanitizeFilename(filenames[i]);
                if (cleaned.empty())
                    return serveError(403);

                std::string target = uploadPath;
                if (!target.empty() && target[target.size() - 1] != '/')
                    target += '/';

                target += cleaned;

                Response res = FileHandler::post(
                    target,
                    contents[i],
                    request.getConnectionHeader());

                if (res.getStatusCode() >= 400)
                    return res;
            }

            Response resp;
            resp.setStatus(201);
            resp.setHeader("Content-Type", "text/plain");
            resp.setHeader("Connection", request.getConnectionHeader());
            resp.setBody("Uploaded");
            return resp;
        }

        std::string filename = sanitizeFilename(baseName(request_path));
        if (filename.empty())
            return serveError(403);

        std::string target = uploadPath;
        if (!target.empty() && target[target.size() - 1] != '/')
            target += '/';

        target += filename;

        return FileHandler::post(
            target,
            request.getBody(),
            request.getConnectionHeader());
    }
    if (request.getMethod() == "DELETE")
    {
        std::string full_path = buildPath(server, *location, request_path);

        return FileHandler::del(
            full_path,
            request.getConnectionHeader());
    }
    if (request.getMethod() == "GET")
    {
        std::string full_path = buildPath(server, *location, request_path);

        if (full_path.find(server.root) != 0)
        {
            if (!server.root.empty() && server.root[server.root.size() - 1] != '/')
                full_path = server.root + "/" + full_path;
            else
                full_path = server.root + full_path;
        }

        if (!fileExists(full_path))
            return serveError(404);

        if (isDirectory(full_path))
        {
            if (!location->index.empty())
            {
                std::string index_path = full_path;
                if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
                    index_path += "/";
                index_path += location->index;

                if (fileExists(index_path))
                    full_path = index_path;
                else
                {
                    if (location->autoindex == "on")
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
                if (location->autoindex == "on")
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
        if (isCgiExtension(ext, location->cgi_ext))
            return serveCgi(*location, request, full_path);

        return serveStaticFile(full_path);
    }

    return serveError(405);
}