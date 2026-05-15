#include "../../includes/MethodHandler.hpp"
#include "../../includes/FileHandler.hpp"
#include "../../includes/CGI.hpp"

#include <unistd.h>
#include <sstream>
#include <cstdlib>

namespace
{
    bool startsWith(const std::string &value, const std::string &prefix)
    {
        return value.compare(0, prefix.size(), prefix) == 0;
    }

    std::string stripQuery(const std::string &path)
    {
        size_t pos = path.find('?');
        if (pos == std::string::npos)
            return path;
        return path.substr(0, pos);
    }

    std::string trim(const std::string &value)
    {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && (value[start] == ' ' || value[start] == '\t'))
            start++;

        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
            end--;

        return value.substr(start, end - start);
    }

    std::string locationPathToken(const std::string &raw, bool &isExact)
    {
        isExact = false;
        std::string cleaned = trim(raw);
        if (cleaned.empty())
            return "/";

        std::string first;
        std::string second;
        std::istringstream iss(cleaned);
        iss >> first;
        if (iss >> second)
        {
            if (first == "=")
            {
                isExact = true;
                return second;
            }
            if (first == "^~" || first == "~" || first == "~*")
                return second;
        }

        return first;
    }

    const Location *selectLocation(const Server &server, const std::string &reqPath)
    {
        const Location *best = NULL;
        size_t bestLen = 0;

        for (size_t i = 0; i < server.locations.size(); i++)
        {
            bool isExact = false;
            std::string locPath = locationPathToken(server.locations[i].path, isExact);

            if (locPath.empty())
                locPath = "/";

            if (isExact)
            {
                if (reqPath == locPath)
                    return &server.locations[i];
                continue;
            }

            if (startsWith(reqPath, locPath))
            {
                if (locPath.size() > bestLen)
                {
                    bestLen = locPath.size();
                    best = &server.locations[i];
                }
            }
        }

        return best;
    }

    std::string joinMethods(const std::vector<std::string> &methods)
    {
        std::string result;
        for (size_t i = 0; i < methods.size(); i++)
        {
            if (i > 0)
                result += ", ";
            result += methods[i];
        }
        return result;
    }

    bool isMethodAllowed(const Location *loc, const std::string &method, std::string &allowHeader)
    {
        if (!loc || loc->methods.empty())
            return true;

        for (size_t i = 0; i < loc->methods.size(); i++)
        {
            if (loc->methods[i] == method)
                return true;
        }

        allowHeader = joinMethods(loc->methods);
        return false;
    }

    void applyIndexIfNeeded(std::string &path, const std::string &index)
    {
        if (index.empty())
            return;
        if (!path.empty() && path[path.size() - 1] == '/')
            path += index;
    }

    void resolveRoute(const Server &server, const std::string &reqPath, std::string &root, std::string &path, const Location *&loc)
    {
        root = server.root;
        path = reqPath;
        loc = selectLocation(server, reqPath);

        if (!loc)
        {
            applyIndexIfNeeded(path, server.index);
            return;
        }

        if (!loc->root.empty())
        {
            root = loc->root;
            bool isExact = false;
            std::string locPath = locationPathToken(loc->path, isExact);
            if (!locPath.empty() && startsWith(reqPath, locPath))
            {
                std::string suffix = reqPath.substr(locPath.size());
                if (suffix.empty())
                    suffix = "/";
                if (!suffix.empty() && suffix[0] != '/')
                    suffix = "/" + suffix;
                path = suffix;
            }
        }

        if (!loc->alias.empty())
        {
            bool isExact = false;
            std::string locPath = locationPathToken(loc->path, isExact);
            if (!locPath.empty() && startsWith(reqPath, locPath))
            {
                std::string suffix = reqPath.substr(locPath.size());
                if (suffix.empty())
                    suffix = "/";
                if (!suffix.empty() && suffix[0] != '/')
                    suffix = "/" + suffix;
                path = suffix;
            }
            root = loc->alias;
        }

        applyIndexIfNeeded(path, loc->index.empty() ? server.index : loc->index);
    }

    std::string pathExtension(const std::string &path)
    {
        size_t dot = path.rfind('.');
        if (dot == std::string::npos)
            return "";
        return path.substr(dot);
    }

    bool resolveCgi(const Location *loc, const std::string &root, const std::string &path, std::string &interpreter, std::string &scriptPath)
    {
        if (!loc || loc->cgi_ext.empty())
            return false;

        std::string ext = pathExtension(path);
        if (ext.empty())
            return false;

        int extIndex = -1;
        for (size_t i = 0; i < loc->cgi_ext.size(); i++)
        {
            if (loc->cgi_ext[i] == ext)
            {
                extIndex = static_cast<int>(i);
                break;
            }
        }

        if (extIndex < 0)
            return false;

        if (loc->cgi_path.size() == loc->cgi_ext.size())
            interpreter = loc->cgi_path[extIndex];
        else if (!loc->cgi_path.empty())
            interpreter = loc->cgi_path[0];

        scriptPath = root + path;
        if (access(scriptPath.c_str(), F_OK) != 0)
            return false;

        return true;
    }

    Response buildCgiResponse(const std::string &cgiOutput, const std::string &connection)
    {
        Response resp;
        resp.setHeader("Connection", connection);

        if (cgiOutput.empty())
        {
            resp.setStatus(500);
            resp.setBody("CGI execution failed");
            return resp;
        }

        size_t headerEnd = cgiOutput.find("\r\n\r\n");
        size_t separatorSize = 4;
        if (headerEnd == std::string::npos)
        {
            headerEnd = cgiOutput.find("\n\n");
            separatorSize = 2;
        }

        if (headerEnd == std::string::npos)
        {
            resp.setStatus(200);
            resp.setHeader("Content-Type", "text/plain");
            resp.setBody(cgiOutput);
            return resp;
        }

        std::string headers = cgiOutput.substr(0, headerEnd);
        std::string body = cgiOutput.substr(headerEnd + separatorSize);

        int statusCode = 200;
        std::istringstream headerStream(headers);
        std::string line;
        while (std::getline(headerStream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);

            if (line.empty())
                continue;

            size_t sep = line.find(':');
            if (sep == std::string::npos)
                continue;

            std::string key = line.substr(0, sep);
            std::string value = line.substr(sep + 1);

            if (!value.empty() && value[0] == ' ')
                value.erase(0, 1);

            if (key == "Status")
                statusCode = std::atoi(value.c_str());
            else
                resp.setHeader(key, value);
        }

        resp.setStatus(statusCode);
        resp.setBody(body);
        return resp;
    }
}

namespace
{
    int parseStatusCode(const std::string &status)
    {
        if (status.size() < 3)
            return 400;

        int code = 0;
        for (size_t i = 0; i < 3; i++)
        {
            if (status[i] < '0' || status[i] > '9')
                return 400;
            code = code * 10 + (status[i] - '0');
        }

        return code;
    }
}

Response MethodHandler::handle(const Request &req, const Server &server)
{
    if (req.hasError())
    {
        Response resp;
        resp.setStatus(parseStatusCode(req.getErrorStatus()));
        resp.setHeader("Connection", req.getConnectionHeader());
        return resp;
    }

    if (req.getBody().size() > static_cast<size_t>(server.client_max_body_size))
    {
        Response resp;
        resp.setStatus(413);
        resp.setHeader("Connection", req.getConnectionHeader());
        return resp;
    }

    std::string root;
    std::string path = stripQuery(req.getPath());
    const Location *loc = NULL;
    resolveRoute(server, path, root, path, loc);

    if (loc && !loc->redirect.empty())
    {
        Response resp;
        resp.setStatus(loc->redirect_code);
        resp.setHeader("Location", loc->redirect);
        resp.setHeader("Connection", req.getConnectionHeader());
        return resp;
    }

    std::string allowHeader;
    if (!isMethodAllowed(loc, req.getMethod(), allowHeader))
    {
        Response resp;
        resp.setStatus(405);
        resp.setHeader("Allow", allowHeader);
        resp.setHeader("Connection", req.getConnectionHeader());
        return resp;
    }

    if (req.getMethod() == "GET")
        return handleGet(req, server, root, path, loc);

    if (req.getMethod() == "POST")
        return handlePost(req, server, root, path, loc);

    if (req.getMethod() == "DELETE")
        return handleDelete(req, server, root, path, loc);

    Response resp;
    resp.setStatus(405);
    resp.setHeader("Connection", req.getConnectionHeader());
    return resp;
}

Response MethodHandler::handleGet(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc)
{
    (void)server;
    std::string interpreter;
    std::string scriptPath;
    if (resolveCgi(loc, root, path, interpreter, scriptPath))
    {
        if (interpreter.empty())
        {
            Response resp;
            resp.setStatus(500);
            resp.setHeader("Connection", req.getConnectionHeader());
            resp.setBody("CGI interpreter not configured");
            return resp;
        }

        CGI cgi;
        cgi.setInterpreter(interpreter);
        cgi.setScriptPath(scriptPath);
        std::string output = cgi.execute(req);
        return buildCgiResponse(output, req.getConnectionHeader());
    }

    return FileHandler::get(path, req.getConnectionHeader(), root.empty() ? server.root : root);
}

Response MethodHandler::handlePost(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc)
{
    (void)server;
    std::string interpreter;
    std::string scriptPath;
    if (resolveCgi(loc, root, path, interpreter, scriptPath))
    {
        if (interpreter.empty())
        {
            Response resp;
            resp.setStatus(500);
            resp.setHeader("Connection", req.getConnectionHeader());
            resp.setBody("CGI interpreter not configured");
            return resp;
        }

        CGI cgi;
        cgi.setInterpreter(interpreter);
        cgi.setScriptPath(scriptPath);
        std::string output = cgi.execute(req);
        return buildCgiResponse(output, req.getConnectionHeader());
    }

    return FileHandler::post(path, req.getBody(), req.getConnectionHeader(), root.empty() ? server.root : root);
}

Response MethodHandler::handleDelete(const Request &req, const Server &server, const std::string &root, const std::string &path, const Location *loc)
{
    (void)server;
    (void)loc;
    return FileHandler::del(path, req.getConnectionHeader(), root.empty() ? server.root : root);
}