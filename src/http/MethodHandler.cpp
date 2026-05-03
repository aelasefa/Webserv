#include "../../includes/MethodHandler.hpp"
#include "../../includes/FileHandler.hpp"

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

Response MethodHandler::handle(const Request &req)
{
    if (req.hasError())
    {
        Response resp;
        resp.setStatus(parseStatusCode(req.getErrorStatus()));
        resp.setHeader("Connection", req.getConnectionHeader());
        return resp;
    }

    if (req.getMethod() == "GET")
        return handleGet(req);

    if (req.getMethod() == "POST")
        return handlePost(req);

    if (req.getMethod() == "DELETE")
        return handleDelete(req);

    Response resp;
    resp.setStatus(405);
    resp.setHeader("Connection", req.getConnectionHeader());
    return resp;
}

Response MethodHandler::handleGet(const Request &req)
{
    return FileHandler::get(req.getPath(), req.getConnectionHeader());
}

Response MethodHandler::handlePost(const Request &req)
{
    return FileHandler::post(req.getPath(), req.getBody(), req.getConnectionHeader());
}

Response MethodHandler::handleDelete(const Request &req)
{
    return FileHandler::del(req.getPath(), req.getConnectionHeader());
}