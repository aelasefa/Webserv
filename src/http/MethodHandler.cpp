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

    bool isCgiRequest(const Request &req, std::string &scriptPath)
    {
        std::string cleanPath = stripQuery(req.getPath());
        if (!startsWith(cleanPath, "/cgi-bin/"))
            return false;

        std::string localPath = cleanPath.substr(std::string("/cgi-bin").size());
        if (localPath.empty())
            return false;

        scriptPath = std::string("./cgi") + localPath;
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

    if (req.getMethod() == "GET")
        return handleGet(req, server);

    if (req.getMethod() == "POST")
        return handlePost(req, server);

    if (req.getMethod() == "DELETE")
        return handleDelete(req, server);

    Response resp;
    resp.setStatus(405);
    resp.setHeader("Connection", req.getConnectionHeader());
    return resp;
}

Response MethodHandler::handleGet(const Request &req, const Server &server)
{
    std::string scriptPath;
    if (isCgiRequest(req, scriptPath))
    {
        CGI cgi;
        cgi.setInterpreter("/usr/bin/python3");
        cgi.setScriptPath(scriptPath);
        std::string output = cgi.execute(req);
        return buildCgiResponse(output, req.getConnectionHeader());
    }

    return FileHandler::get(req.getPath(), req.getConnectionHeader(), server.root);
}

Response MethodHandler::handlePost(const Request &req, const Server &server)
{
    std::string scriptPath;
    if (isCgiRequest(req, scriptPath))
    {
        CGI cgi;
        cgi.setInterpreter("/usr/bin/python3");
        cgi.setScriptPath(scriptPath);
        std::string output = cgi.execute(req);
        return buildCgiResponse(output, req.getConnectionHeader());
    }

    return FileHandler::post(req.getPath(), req.getBody(), req.getConnectionHeader(), server.root);
}

Response MethodHandler::handleDelete(const Request &req, const Server &server)
{
    return FileHandler::del(req.getPath(), req.getConnectionHeader(), server.root);
}