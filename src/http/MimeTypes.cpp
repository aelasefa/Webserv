#include "../../includes/MimeTypes.hpp"

std::string getExtension(const std::string &path)
{
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return "";
    return path.substr(dot);
}

std::string mimeTypeFromPath(const std::string &path)
{
    std::string ext = getExtension(path);
    if (ext == ".html" || ext == ".htm")
        return "text/html";
    if (ext == ".css")
        return "text/css";
    if (ext == ".js")
        return "application/javascript";
    if (ext == ".png")
        return "image/png";
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".ico")
        return "image/x-icon";
    if (ext == ".svg")
        return "image/svg+xml";
    if (ext == ".txt")
        return "text/plain";
    if (ext == ".json")
        return "application/json";
    if (ext == ".xml")
        return "application/xml";
    if (ext == ".pdf")
        return "application/pdf";
    if (ext == ".zip")
        return "application/zip";
    if (ext == ".mp4")
        return "video/mp4";
    if (ext == ".mp3")
        return "audio/mpeg";
    return "application/octet-stream";
}