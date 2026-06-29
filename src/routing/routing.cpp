#include "../../includes/Router.hpp"
#include "../../includes/FileUtils.hpp"
#include "../../includes/HttpUtils.hpp"



static std::string stripQueryAndFragment(const std::string &path)
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

static std::string baseName(const std::string &path)
{
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos)
        return path;
    if (pos + 1 >= path.size())
        return "";
    return path.substr(pos + 1);
}


Response Router::serveError(int code)
{
    Response response;
    response.setStatus(code);
    response.setHeader("Content-Type", "text/html");
    std::string body = "<html><body><h1>" + StringUtils::toString(code)
                     + " Error</h1></body></html>";
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
    response.setFileBody(path, static_cast<size_t>(st.st_size));
    return response;
}

#define CGI_PENDING_STATUS 0

Response Router::serveCgi(const Location &location,
                          const Request  &request,
                          const std::string &script_path)
{
    (void)request;
    std::string ext = getExtension(script_path);
    std::string interpreter =
        findInterpreter(ext, location.cgi_ext, location.cgi_path);

    if (interpreter.empty())
        return serveError(500);

    Response pending;
    pending.setStatus(CGI_PENDING_STATUS);
    pending.setHeader("X-CGI-Script",      script_path);
    pending.setHeader("X-CGI-Interpreter", interpreter);
    return pending;
}

std::string Router::buildPath(const Server& server, const Location& location,
                              const std::string& request_path)
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
    return mimeTypeFromPath(path);
}

bool Router::isMethodAllowed(const std::string& method,
                              const std::vector<std::string>& allowed)
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

bool Router::isCgiExtension(const std::string& ext,
                             const std::vector<std::string>& cgi_ext)
{
    for (size_t i = 0; i < cgi_ext.size(); i++)
    {
        if (cgi_ext[i] == ext)
            return true;
    }
    return false;
}

std::string Router::findInterpreter(const std::string& ext,
                                    const std::vector<std::string>& cgi_ext,
                                    const std::vector<std::string>& cgi_path)
{
    for (size_t i = 0; i < cgi_ext.size() && i < cgi_path.size(); i++)
    {
        if (cgi_ext[i] == ext)
            return cgi_path[i];
    }
    return "";
}

std::string Router::generateDirectoryListing(const std::string& full_path,
                                              const std::string& request_path)
{
    DIR* dir = opendir(full_path.c_str());
    if (!dir)
        return "";

    std::string html = "<html><head><title>Index of " + StringUtils::htmlEscape(request_path)
                     + "</title></head><body>\n";
    html += "<h1>Index of " + StringUtils::htmlEscape(request_path) + "</h1><hr><pre>\n";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == ".")
            continue;

        std::string link = request_path;
        if (link.empty() || link[link.size() - 1] != '/')
            link += "/";
        link += name;

        html += "<a href=\"" + StringUtils::htmlEscape(link) + "\">" + StringUtils::htmlEscape(name) + "</a>\n";
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

    // Check for path traversal
    if (request_path == ".." ||
        request_path.find("../") == 0 ||
        request_path.find("/../") != std::string::npos ||
        (request_path.size() >= 3 && request_path.compare(request_path.size() - 3, 3, "/..") == 0))
    {
        return serveError(400);
    }

    for (size_t i = 0; i < server.locations.size(); i++)
    {
        const Location& loc = server.locations[i];
        std::string loc_path = loc.path;
        bool is_exact  = false;
        bool is_prefix = true;

        if (!loc_path.empty() && loc_path[0] == '=')
        {
            is_exact  = true;
            is_prefix = false;
            loc_path  = StringUtils::trim(loc_path.substr(1));
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
    if (effectiveBodyLimit > 0 &&
        request.getBody().size() > static_cast<size_t>(effectiveBodyLimit))
        return serveError(413);

    if (location && !isMethodAllowed(request.getMethod(), location->methods))
    {
        Response r = serveError(405);
        std::string allow;
        for (size_t m = 0; m < location->methods.size(); ++m) {
            if (m > 0) allow += ", ";
            allow += location->methods[m];
        }
        r.setHeader("Allow", allow);
        return r;
    }

    if (location && !location->redirect.empty())
        return serveRedirect(location->redirect_code, location->redirect);

    if (request.getMethod() == "POST")
    {
        if (location)
        {
            std::string full_path = buildPath(server, *location, request_path);
            std::string ext       = getExtension(full_path);

            if (isCgiExtension(ext, location->cgi_ext))
            {
                if (!fileExists(full_path))
                    return serveError(404);
                return serveCgi(*location, request, full_path);
            }
        }

        bool uploadEnabled    = server.upload_enable;
        std::string uploadPath = server.upload_path;

        if (location && location->upload_enable_set)
            uploadEnabled = location->upload_enable;
        if (location && location->upload_path_set)
            uploadPath = location->upload_path;

        if (!uploadEnabled)
            return serveError(403);
        if (uploadPath.empty())
            return serveError(403);
        if (!FileHandler::ensureDir(uploadPath))
            return serveError(500);

        std::string contentType = request.getHeader("Content-Type");
        std::string boundary    = HttpUtils::extractMultipartBoundary(contentType);

        if (!boundary.empty())
        {
            std::vector<std::string> filenames;
            std::vector<std::string> contents;

            if (!HttpUtils::extractMultipartFiles(request.getBody(), boundary, filenames, contents))
                return serveError(400);
            if (filenames.empty())
                return serveError(400);

            for (size_t i = 0; i < filenames.size(); i++)
            {
                std::string cleaned = FileUtils::sanitizeFilename(filenames[i]);
                if (cleaned.empty())
                    return serveError(403);

                std::string target = uploadPath;
                if (!target.empty() && target[target.size() - 1] != '/')
                    target += '/';
                target += cleaned;

                Response res = FileHandler::post(target, contents[i],
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

        std::string filename = FileUtils::sanitizeFilename(baseName(request_path));
        if (filename.empty())
        {
            static int counter = 0;
            std::stringstream ss;
            ss << "upload_" << time(NULL) << "_" << counter++;
            filename = ss.str();
        }

        std::string target = uploadPath;
        if (!target.empty() && target[target.size() - 1] != '/')
            target += '/';
        target += filename;

        return FileHandler::post(target, request.getBody(),
                                 request.getConnectionHeader());
    }

    if (request.getMethod() == "DELETE")
    {
        std::string full_path = buildPath(server, *location, request_path);
        return FileHandler::del(full_path, request.getConnectionHeader());
    }

    if (request.getMethod() == "GET")
    {
        std::string full_path = buildPath(server, *location, request_path);

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

    Response r = serveError(405);
    if (location) {
        std::string allow;
        for (size_t m = 0; m < location->methods.size(); ++m) {
            if (m > 0) allow += ", ";
            allow += location->methods[m];
        }
        r.setHeader("Allow", allow);
    }
    return r;
}