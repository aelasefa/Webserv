#include "../../includes/MethodHandler.hpp"
#include "../../includes/FileHandler.hpp"

std::string MethodHandler::handle(const Request &req)
{
    if (req.hasError())
    {
        std::string status = req.getErrorStatus().empty() ? "400 Bad Request" : req.getErrorStatus();
        return "HTTP/1.1 " + status + "\r\nConnection: " + req.getConnectionHeader() + "\r\nContent-Length: 0\r\n\r\n";
    }

    if (req.getMethod() == "GET")
        return handleGet(req);

    if (req.getMethod() == "POST")
        return handlePost(req);

    if (req.getMethod() == "DELETE")
        return handleDelete(req);

    return "HTTP/1.1 405 Method Not Allowed\r\nConnection: " + req.getConnectionHeader() + "\r\nContent-Length: 0\r\n\r\n";
}

std::string MethodHandler::handleGet(const Request &req)
{
    return FileHandler::get(req.getPath(), req.getConnectionHeader());
}

std::string MethodHandler::handlePost(const Request &req)
{
    return FileHandler::post(req.getPath(), req.getBody(), req.getConnectionHeader());
}

std::string MethodHandler::handleDelete(const Request &req)
{
    return FileHandler::del(req.getPath(), req.getConnectionHeader());
}