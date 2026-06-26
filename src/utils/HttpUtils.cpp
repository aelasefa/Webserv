#include "../../includes/HttpUtils.hpp"
#include "../../includes/StringUtils.hpp"
#include "../../includes/MimeTypes.hpp"
#include <sstream>

int HttpUtils::parseStatusCode(const std::string &status)
{
    if (status.size() < 3)
        return 500;

    int code = 0;
    for (size_t i = 0; i < 3; ++i)
    {
        if (status[i] < '0' || status[i] > '9')
            return 500;
        code = code * 10 + (status[i] - '0');
    }
    return code;
}

std::string HttpUtils::extractMultipartBoundary(const std::string &contentType)
{
    std::string lower = StringUtils::toLower(contentType);
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
    boundary = StringUtils::trim(boundary);
    if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
        boundary = boundary.substr(1, boundary.size() - 2);
    return boundary;
}

std::string HttpUtils::extractFilenameFromHeaders(const std::string &headers)
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

        std::string key = StringUtils::toLower(StringUtils::trim(line.substr(0, sep)));
        std::string value = StringUtils::trim(line.substr(sep + 1));

        if (key != "content-disposition")
            continue;

        std::string lowerValue = StringUtils::toLower(value);
        size_t fnPos = lowerValue.find("filename=");
        if (fnPos == std::string::npos)
            return "";

        std::string filename = value.substr(fnPos + 9);
        size_t semi = filename.find(';');
        if (semi != std::string::npos)
            filename = filename.substr(0, semi);
        filename = StringUtils::trim(filename);
        if (filename.size() >= 2 && filename[0] == '"' && filename[filename.size() - 1] == '"')
            filename = filename.substr(1, filename.size() - 2);

        return filename;
    }
    return "";
}

bool HttpUtils::extractMultipartFiles(const std::string &body, const std::string &boundary,
                                      std::vector<std::string> &filenames, std::vector<std::string> &contents)
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

std::string HttpUtils::getMimeType(const std::string &path)
{
    return mimeTypeFromPath(path);
}

bool HttpUtils::isValidMethod(const std::string &method)
{
    return (method == "GET" || method == "POST" || method == "DELETE");
}

bool HttpUtils::isValidHttpVersion(const std::string &version)
{
    return (version == "HTTP/1.1" || version == "HTTP/1.0");
}
