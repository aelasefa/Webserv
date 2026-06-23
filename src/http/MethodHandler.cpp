#include "../../includes/MethodHandler.hpp"
#include "../../includes/FileHandler.hpp"
#include "../../includes/CGI.hpp"
#include "../../includes/Utils.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace
{
    bool startsWith(const std::string &value, const std::string &prefix)
    {
        return value.compare(0, prefix.size(), prefix) == 0;
    }

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

    std::string trimSpaces(const std::string &value)
    {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && (value[start] == ' ' || value[start] == '\t'))
            start++;
        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t'))
            end--;

        return value.substr(start, end - start);
    }

    std::string joinPaths(const std::string &left, const std::string &right)
    {
        if (left.empty())
            return right.empty() ? "/" : right;
        if (right.empty())
            return left;
        if (left[left.size() - 1] == '/' && right[0] == '/')
            return left + right.substr(1);
        if (left[left.size() - 1] != '/' && right[0] != '/')
            return left + "/" + right;
        return left + right;
    }
    
    std::string effectiveRoot(const std::string &root)
    {
        if (root.empty())
            throw std::runtime_error("server root not configured");
        return root;
    }

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

    bool normalizeUriPath(const std::string &rawPath, std::string &normalized)
    {
        std::string path = stripQueryAndFragment(rawPath);
        if (path.empty())
            path = "/";
        if (path[0] != '/')
            path.insert(path.begin(), '/');
        if (path.find('\\') != std::string::npos)
            return false;

        std::vector<std::string> segments;
        std::string segment;
        for (size_t i = 0; i <= path.size(); ++i)
        {
            if (i == path.size() || path[i] == '/')
            {
                if (segment == "..")
                {
                    if (segments.empty())
                        return false;
                    segments.pop_back();
                }
                else if (!segment.empty() && segment != ".")
                {
                    segments.push_back(segment);
                }
                segment.clear();
            }
            else
            {
                segment += path[i];
            }
        }

        normalized = "/";
        for (size_t i = 0; i < segments.size(); ++i)
        {
            normalized += segments[i];
            if (i + 1 < segments.size())
                normalized += "/";
        }
        if (!path.empty() && path[path.size() - 1] == '/' && normalized.size() > 1)
            normalized += "/";
        return true;
    }

    bool parseLocationToken(const std::string &raw, std::string &pattern, bool &isExact)
    {
        std::string cleaned = trimSpaces(raw);
        if (cleaned.empty())
            return false;

        isExact = false;
        std::istringstream iss(cleaned);
        std::string first;
        std::string second;
        if (!(iss >> first))
            return false;

        if (first == "=")
        {
            isExact = true;
            if (!(iss >> second))
                return false;
            pattern = second;
            return true;
        }

        if (first == "^~" || first == "~" || first == "~*")
        {
            if (!(iss >> second))
                return false;
            pattern = second;
            return true;
        }

        pattern = first;
        return true;
    }

    const Location *selectLocation(const Server &server, const std::string &requestPath)
    {
        const Location *best = NULL;
        size_t bestLength = 0;

        for (size_t i = 0; i < server.locations.size(); ++i)
        {
            std::string pattern;
            bool isExact = false;
            if (!parseLocationToken(server.locations[i].path, pattern, isExact))
                continue;

            if (pattern.empty())
                pattern = "/";

            if (isExact)
            {
                if (requestPath == pattern)
                    return &server.locations[i];
                continue;
            }

            if (startsWith(requestPath, pattern) && pattern.size() >= bestLength)
            {
                bestLength = pattern.size();
                best = &server.locations[i];
            }
        }

        return best;
    }

    std::string locationPrefix(const Location *loc)
    {
        if (!loc)
            return "";

        std::string pattern;
        bool isExact = false;
        if (!parseLocationToken(loc->path, pattern, isExact))
            return "";
        return pattern;
    }

    std::string resolveFilesystemRelativePath(const Location *loc,
                                              const std::string &requestPath,
                                              bool &matchedLocation)
    {
        matchedLocation = false;
        if (!loc)
            return requestPath;

        std::string prefix = locationPrefix(loc);
        if (prefix.empty())
            return requestPath;

        if (!startsWith(requestPath, prefix))
            return requestPath;

        matchedLocation = true;

        if (!loc->alias.empty())
        {
            std::string suffix = requestPath.substr(prefix.size());
            if (suffix.empty())
                suffix = "/";
            if (!suffix.empty() && suffix[0] != '/')
                suffix.insert(suffix.begin(), '/');
            return suffix;
        }

        return requestPath;
    }

    std::string resolveDocumentPath(const Server &server,
                                    const Location *loc,
                                    const std::string &requestPath,
                                    std::string &docRoot,
                                    bool &matchedLocation)
    {
        docRoot = effectiveRoot(loc && !loc->root.empty() ? loc->root : server.root);
        if (loc && !loc->alias.empty())
            docRoot = loc->alias;

        std::string relative = resolveFilesystemRelativePath(loc, requestPath, matchedLocation);
        if (!relative.empty() && relative[0] == '/' && !docRoot.empty() && docRoot[docRoot.size() - 1] == '/')
            return docRoot + relative.substr(1);
        if (!relative.empty() && relative[0] != '/' && !docRoot.empty() && docRoot[docRoot.size() - 1] != '/')
            return docRoot + "/" + relative;
        return joinPaths(docRoot, relative);
    }

    bool readFile(const std::string &path, std::string &content)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        if (!file.is_open())
            return false;

        std::ostringstream oss;
        oss << file.rdbuf();
        content = oss.str();
        return true;
    }

    bool fileExists(const std::string &path, struct stat *stOut)
    {
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            return false;
        if (stOut)
            *stOut = st;
        return true;
    }

    bool canReadFile(const std::string &path)
    {
        return access(path.c_str(), R_OK) == 0;
    }

    bool canWriteDirectory(const std::string &path)
    {
        return access(path.c_str(), W_OK | X_OK) == 0;
    }

    std::string parentDirectory(const std::string &path)
    {
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos)
            return ".";
        if (pos == 0)
            return "/";
        return path.substr(0, pos);
    }

    std::string basename(const std::string &path)
    {
        size_t pos = path.find_last_of('/');
        if (pos == std::string::npos)
            return path;
        if (pos + 1 >= path.size())
            return "";
        return path.substr(pos + 1);
    }

    std::string htmlEscape(const std::string &value)
    {
        std::string out;
        for (size_t i = 0; i < value.size(); ++i)
        {
            char c = value[i];
            if (c == '&')
                out += "&amp;";
            else if (c == '<')
                out += "&lt;";
            else if (c == '>')
                out += "&gt;";
            else if (c == '"')
                out += "&quot;";
            else
                out += c;
        }
        return out;
    }

    bool writeAll(int fd, const std::string &data)
    {
        size_t writtenTotal = 0;
        while (writtenTotal < data.size())
        {
            ssize_t written = write(fd, data.data() + writtenTotal, data.size() - writtenTotal);
            if (written < 0)
            {
                if (errno == EINTR)
                    continue;
                return false;
            }
            if (written == 0)
                return false;
            writtenTotal += static_cast<size_t>(written);
        }
        return true;
    }

    bool createTempFile(const std::string &targetPath, std::string &tempPath, int &fd)
    {
        for (int i = 0; i < 100; ++i)
        {
            std::ostringstream oss;
            oss << targetPath << ".tmp." << getpid() << "." << i;
            tempPath = oss.str();
            fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
            if (fd >= 0)
                return true;
        }
        return false;
    }

    Response buildResponse(int status,
                           const std::string &body,
                           const std::string &contentType,
                           const std::string &connection)
    {
        Response resp;
        resp.setStatus(status);
        resp.setHeader("Connection", connection);
        resp.setHeader("Content-Type", contentType);
        resp.setBody(body);
        return resp;
    }

    Response serveConfiguredErrorPage(const Server &server, int status, const std::string &connection)
    {
        std::map<int, std::string>::const_iterator it = server.error_pages.find(status);
        if (it != server.error_pages.end())
        {
            std::string pagePath = it->second;
            if (!pagePath.empty())
            {
                std::string resolved = pagePath;
                if (!pagePath.empty() && pagePath[0] != '/')
                    resolved = joinPaths(effectiveRoot(server.root), pagePath);

                std::string body;
                if (readFile(resolved, body))
                    return buildResponse(status, body, "text/html", connection);
            }
        }

        std::string body;
        body = "<html><head><title>" + Utils::toString(status) + "</title></head><body><h1>";
        body += Utils::toString(status);
        body += "</h1></body></html>";
        return buildResponse(status, body, "text/html", connection);
    }

    int parseStatusCode(const std::string &status)
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

    bool methodAllowed(const Location *loc, const std::string &method, std::string &allowHeader)
    {
        if (!loc || loc->methods.empty())
            return true;

        for (size_t i = 0; i < loc->methods.size(); ++i)
        {
            if (loc->methods[i] == method)
                return true;
        }

        allowHeader.clear();
        for (size_t i = 0; i < loc->methods.size(); ++i)
        {
            if (i > 0)
                allowHeader += ", ";
            allowHeader += loc->methods[i];
        }
        return false;
    }

    bool locationAutoindexOn(const Location *loc)
    {
        return loc && loc->autoindex == "on";
    }

    std::string indexFileName(const Server &server, const Location *loc)
    {
        if (loc && !loc->index.empty())
            return loc->index;
        return server.index;
    }

    Response buildDirectoryListing(const std::string &directoryPath,
                                   const std::string &requestPath,
                                   const std::string &connection)
    {
        DIR *dir = opendir(directoryPath.c_str());
        if (!dir)
            return buildResponse(500, "Internal Server Error", "text/plain", connection);

        std::string normalizedRequest = requestPath;
        if (normalizedRequest.empty())
            normalizedRequest = "/";
        if (!normalizedRequest.empty() && normalizedRequest[normalizedRequest.size() - 1] != '/')
            normalizedRequest += "/";

        std::string body = "<html><head><title>Index of " + htmlEscape(normalizedRequest) + "</title></head><body>";
        body += "<h1>Index of " + htmlEscape(normalizedRequest) + "</h1><hr><pre>";

        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;
            if (name == ".")
                continue;

            std::string href = normalizedRequest;
            if (!href.empty() && href[href.size() - 1] != '/')
                href += "/";
            href += name;

            body += "<a href=\"" + htmlEscape(href) + "\">" + htmlEscape(name) + "</a>\n";
        }

        closedir(dir);
        body += "</pre><hr></body></html>";
        return buildResponse(200, body, "text/html", connection);
    }

    Response serveStaticFile(const std::string &path,
                             const std::string &connection,
                             const std::string &requestPath)
    {
        struct stat st;
        if (!fileExists(path, &st))
        {
            if (errno == ENOENT || errno == ENOTDIR)
                return buildResponse(404, "Not Found", "text/plain", connection);
            if (errno == EACCES || errno == EPERM)
                return buildResponse(403, "Forbidden", "text/plain", connection);
            return buildResponse(500, "Internal Server Error", "text/plain", connection);
        }

        if (S_ISDIR(st.st_mode))
        {
            if (!requestPath.empty() && requestPath[requestPath.size() - 1] != '/')
            {
                Response redirect;
                redirect.setStatus(301);
                redirect.setHeader("Connection", connection);
                redirect.setHeader("Location", requestPath + "/");
                return redirect;
            }
            return buildResponse(403, "Forbidden", "text/plain", connection);
        }

        if (!canReadFile(path))
            return buildResponse(403, "Forbidden", "text/plain", connection);

        std::string content;
        if (!readFile(path, content))
        {
            if (errno == EACCES || errno == EPERM)
                return buildResponse(403, "Forbidden", "text/plain", connection);
            return buildResponse(500, "Internal Server Error", "text/plain", connection);
        }

        return buildResponse(200, content, mimeTypeFromPath(path), connection);
    }

    Response serveDirectory(const Server &server,
                            const Location *loc,
                            const std::string &directoryPath,
                            const std::string &requestPath,
                            const std::string &connection)
    {
        std::string indexName = indexFileName(server, loc);
        if (!indexName.empty())
        {
            std::string indexPath = joinPaths(directoryPath, indexName);
            struct stat st;
            if (fileExists(indexPath, &st) && S_ISREG(st.st_mode))
                return serveStaticFile(indexPath, connection, requestPath);
        }

        if (locationAutoindexOn(loc))
            return buildDirectoryListing(directoryPath, requestPath, connection);

        return buildResponse(403, "Forbidden", "text/plain", connection);
    }

    bool isCgiTarget(const Location *loc, const std::string &path, std::string &interpreter, std::string &scriptPath)
    {
        if (!loc || loc->cgi_ext.empty())
            return false;

        std::string ext = getExtension(path);
        if (ext.empty())
            return false;

        int matchIndex = -1;
        for (size_t i = 0; i < loc->cgi_ext.size(); ++i)
        {
            if (loc->cgi_ext[i] == ext)
            {
                matchIndex = static_cast<int>(i);
                break;
            }
        }

        if (matchIndex < 0)
            return false;

        if (loc->cgi_path.size() == loc->cgi_ext.size())
            interpreter = loc->cgi_path[matchIndex];
        else if (!loc->cgi_path.empty())
            interpreter = loc->cgi_path[0];

        scriptPath = path;
        return !interpreter.empty();
    }

    Response parseCgiOutput(const std::string &output, const std::string &connection)
    {
        if (output.empty())
            return buildResponse(502, "Bad Gateway", "text/plain", connection);

        size_t headerEnd = output.find("\r\n\r\n");
        size_t separatorSize = 4;
        if (headerEnd == std::string::npos)
        {
            headerEnd = output.find("\n\n");
            separatorSize = 2;
        }

        if (headerEnd == std::string::npos)
            return buildResponse(200, output, "text/plain", connection);

        std::string headerBlock = output.substr(0, headerEnd);
        std::string body = output.substr(headerEnd + separatorSize);

        int statusCode = 200;
        std::string contentType = "text/html";
        std::map<std::string, std::string> headers;

        std::istringstream stream(headerBlock);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);
            if (line.empty())
                continue;

            size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;

            std::string key = Utils::toLower(Utils::trim(line.substr(0, colon)));
            std::string value = Utils::trim(line.substr(colon + 1));
            if (key == "status")
            {
                statusCode = parseStatusCode(value);
                if (statusCode < 100 || statusCode > 599)
                    statusCode = 500;
            }
            else if (key == "content-type")
            {
                contentType = value;
            }
            else
            {
                headers[key] = value;
            }
        }

        Response response;
        response.setStatus(statusCode);
        response.setHeader("Connection", connection);
        response.setHeader("Content-Type", contentType);
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
            response.setHeader(it->first, it->second);
        response.setBody(body);
        return response;
    }

    Response handleCgi(const Request &req,
                       const std::string &connection,
                       const std::string &scriptPath,
                       const std::string &interpreter)
    {
        if (scriptPath.empty())
            return buildResponse(404, "Not Found", "text/plain", connection);

        struct stat st;
        if (!fileExists(scriptPath, &st))
            return buildResponse(404, "Not Found", "text/plain", connection);

        if (!interpreter.empty() && access(interpreter.c_str(), X_OK) != 0)
            return buildResponse(500, "Internal Server Error", "text/plain", connection);

        try
        {
            CGI cgi;
            cgi.setInterpreter(interpreter);
            cgi.setScriptPath(scriptPath);
            std::string output = cgi.execute(req);
            return parseCgiOutput(output, connection);
        }
        catch (const std::exception &)
        {
            return buildResponse(502, "Bad Gateway", "text/plain", connection);
        }
    }

    bool parseMultipartBoundary(const std::string &contentType, std::string &boundary)
    {
        std::string lower = Utils::toLower(contentType);
        if (lower.find("multipart/form-data") == std::string::npos)
            return false;

        size_t boundaryPos = lower.find("boundary=");
        if (boundaryPos == std::string::npos)
            return false;

        boundary = contentType.substr(boundaryPos + 9);
        size_t semi = boundary.find(';');
        if (semi != std::string::npos)
            boundary = boundary.substr(0, semi);
        boundary = trimSpaces(boundary);
        if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size() - 1] == '"')
            boundary = boundary.substr(1, boundary.size() - 2);
        return !boundary.empty();
    }

    std::string extractFilenameFromHeaders(const std::string &headers)
    {
        std::istringstream iss(headers);
        std::string line;
        while (std::getline(iss, line))
        {
            if (!line.empty() && line[line.size() - 1] == '\r')
                line.erase(line.size() - 1);

            size_t colon = line.find(':');
            if (colon == std::string::npos)
                continue;

            std::string key = Utils::toLower(Utils::trim(line.substr(0, colon)));
            if (key != "content-disposition")
                continue;

            std::string value = Utils::trim(line.substr(colon + 1));
            std::string lowerValue = Utils::toLower(value);
            size_t filenamePos = lowerValue.find("filename=");
            if (filenamePos == std::string::npos)
                return "";

            std::string filename = value.substr(filenamePos + 9);
            size_t semi = filename.find(';');
            if (semi != std::string::npos)
                filename = filename.substr(0, semi);
            filename = trimSpaces(filename);
            if (filename.size() >= 2 && filename[0] == '"' && filename[filename.size() - 1] == '"')
                filename = filename.substr(1, filename.size() - 2);
            return filename;
        }
        return "";
    }

    bool extractMultipartFiles(const std::string &body,
                               const std::string &boundary,
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
    static std::string sanitizeFilename(const std::string &raw)
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
    Response storeUploadedFile(const std::string &targetDirectory,
                               const std::string &filename,
                               const std::string &contents,
                               const std::string &connection)
    {
        if (filename.empty())
            return buildResponse(400, "Bad Request", "text/plain", connection);

        if (!canWriteDirectory(targetDirectory))
            return buildResponse(403, "Forbidden", "text/plain", connection);

        std::string targetPath = joinPaths(targetDirectory, filename);
        if (access(targetPath.c_str(), F_OK) == 0)
            return buildResponse(403, "Forbidden", "text/plain", connection);

        std::string tempPath;
        int tempFd = -1;
        if (!createTempFile(targetPath, tempPath, tempFd))
            return buildResponse(500, "Internal Server Error", "text/plain", connection);

        bool wrote = writeAll(tempFd, contents);
        close(tempFd);
        if (!wrote)
        {
            std::remove(tempPath.c_str());
            return buildResponse(500, "Internal Server Error", "text/plain", connection);
        }

        if (std::rename(tempPath.c_str(), targetPath.c_str()) != 0)
        {
            std::remove(tempPath.c_str());
            return buildResponse(500, "Internal Server Error", "text/plain", connection);
        }

        return buildResponse(201, "Created", "text/plain", connection);
    }

    Response handleUpload(const Request &req,
                          const Server &server,
                          const Location *loc,
                          const std::string &requestPath,
                          const std::string &connection)
    {
        bool uploadEnabled = server.upload_enable;
        std::string uploadPath = server.upload_path;
        if (!uploadPath.empty() && uploadPath[0] != '/')
            uploadPath = joinPaths(server.root, uploadPath);

        if (loc)
        {
            if (loc->upload_enable_set)
                uploadEnabled = loc->upload_enable;
            if (loc->upload_path_set)
                uploadPath = loc->upload_path;
        }

        if (!uploadEnabled || uploadPath.empty())
            return buildResponse(403, "Forbidden", "text/plain", connection);

        std::string contentType = req.getHeader("Content-Type");
        std::string boundary;
        if (parseMultipartBoundary(contentType, boundary))
        {
            std::vector<std::string> filenames;
            std::vector<std::string> contents;
            if (!extractMultipartFiles(req.getBody(), boundary, filenames, contents))
                return buildResponse(400, "Bad Request", "text/plain", connection);
            if (filenames.empty())
                return buildResponse(400, "Bad Request", "text/plain", connection);

            for (size_t i = 0; i < filenames.size(); ++i)
            {
                std::string cleaned = sanitizeFilename(filenames[i]);
                if (cleaned.empty())
                    return buildResponse(400, "Bad Request", "text/plain", connection);
                Response res = storeUploadedFile(uploadPath, cleaned, contents[i], connection);
                if (res.getStatusCode() >= 400)
                    return res;
            }

            return buildResponse(201, "Created", "text/plain", connection);
        }

        std::string fileName = sanitizeFilename(basename(requestPath));
        if (fileName.empty())
            return buildResponse(400, "Bad Request", "text/plain", connection);

        return storeUploadedFile(uploadPath, fileName, req.getBody(), connection);
    }

}

Response MethodHandler::handle(const Request &req, const Server &server)
{
    if (req.hasError())
    {
        int status = parseStatusCode(req.getErrorStatus());
        return serveConfiguredErrorPage(server, status, req.getConnectionHeader());
    }

    std::string normalizedPath;
    if (!normalizeUriPath(req.getPath(), normalizedPath))
        return serveConfiguredErrorPage(server, 403, req.getConnectionHeader());

    const Location *loc = selectLocation(server, normalizedPath);

    std::string allowHeader;
    if (!methodAllowed(loc, req.getMethod(), allowHeader))
    {
        Response resp = serveConfiguredErrorPage(server, 405, req.getConnectionHeader());
        resp.setHeader("Allow", allowHeader);
        return resp;
    }

    if (req.getMethod() != "GET" && req.getMethod() != "POST" && req.getMethod() != "DELETE")
    {
        Response res;
        res.setStatus(405);
        res.setHeader("Allow", "GET, POST, DELETE");
        res.setBody("Method Not Allowed");
        return res;
    }

    if (req.getBody().size() > static_cast<size_t>(server.client_max_body_size))
        return serveConfiguredErrorPage(server, 413, req.getConnectionHeader());

    std::string docRoot;
    bool matchedLocation = false;
    std::string filePath = resolveDocumentPath(server, loc, normalizedPath, docRoot, matchedLocation);
    (void)matchedLocation;

    if (loc && !loc->redirect.empty())
    {
        Response resp;
        resp.setStatus(loc->redirect_code);
        resp.setHeader("Connection", req.getConnectionHeader());
        resp.setHeader("Location", loc->redirect);
        return resp;
    }

    if (req.getMethod() == "GET")
        return handleGet(req, server, docRoot, filePath, loc);
    if (req.getMethod() == "POST")
        return handlePost(req, server, docRoot, filePath, loc);
    return handleDelete(req, server, docRoot, filePath, loc);
}

Response MethodHandler::handleGet(const Request &req,
                                  const Server &server,
                                  const std::string &root,
                                  const std::string &path,
                                  const Location *loc)
{
    (void)root;

    std::string connection = req.getConnectionHeader();
    std::string interpreter;
    std::string scriptPath;

    if (isCgiTarget(loc, path, interpreter, scriptPath))
    {
        if (access(scriptPath.c_str(), F_OK) != 0)
            return serveConfiguredErrorPage(server, 404, connection);
        return handleCgi(req, connection, scriptPath, interpreter);
    }

    struct stat st;
    if (!fileExists(path, &st))
    {
        if (errno == ENOENT || errno == ENOTDIR)
            return serveConfiguredErrorPage(server, 404, connection);
        if (errno == EACCES || errno == EPERM)
            return serveConfiguredErrorPage(server, 403, connection);
        return serveConfiguredErrorPage(server, 500, connection);
    }
    if (S_ISDIR(st.st_mode))
        return serveDirectory(server, loc, path, req.getPath(), connection);

    if (!canReadFile(path))
        return serveConfiguredErrorPage(server, 403, connection);

    return serveStaticFile(path, connection, req.getPath());
}

Response MethodHandler::handlePost(const Request &req,
                                   const Server &server,
                                   const std::string &root,
                                   const std::string &path,
                                   const Location *loc)
{
    (void)root;

    std::string connection = req.getConnectionHeader();
    std::string interpreter;
    std::string scriptPath;

    if (isCgiTarget(loc, path, interpreter, scriptPath))
    {
        if (access(scriptPath.c_str(), F_OK) != 0)
            return serveConfiguredErrorPage(server, 404, connection);
        return handleCgi(req, connection, scriptPath, interpreter);
    }

    return handleUpload(req, server, loc, req.getPath(), connection);
}

Response MethodHandler::handleDelete(const Request &req,
                                     const Server &server,
                                     const std::string &root,
                                     const std::string &path,
                                     const Location *loc)
{
    (void)root;
    (void)loc;

    std::string connection = req.getConnectionHeader();
    struct stat st;
    if (!fileExists(path, &st))
    {
        if (errno == ENOENT || errno == ENOTDIR)
            return serveConfiguredErrorPage(server, 404, connection);
        if (errno == EACCES || errno == EPERM)
            return serveConfiguredErrorPage(server, 403, connection);
        return serveConfiguredErrorPage(server, 500, connection);
    }

    if (!S_ISREG(st.st_mode))
        return serveConfiguredErrorPage(server, 403, connection);

    std::string dir = parentDirectory(path);
    if (!canWriteDirectory(dir))
        return serveConfiguredErrorPage(server, 403, connection);

    if (std::remove(path.c_str()) != 0)
    {
        if (errno == ENOENT || errno == ENOTDIR)
            return serveConfiguredErrorPage(server, 404, connection);
        if (errno == EACCES || errno == EPERM)
            return serveConfiguredErrorPage(server, 403, connection);
        return serveConfiguredErrorPage(server, 500, connection);
    }

    Response resp;
    resp.setStatus(204);
    resp.setHeader("Connection", connection);
    return resp;
}
