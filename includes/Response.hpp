#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include "Request.hpp"

class Response
{
    private:
        int statusCode;
        std::string statusMessage;
        std::vector< std::pair<std::string, std::string> > headers;
        std::string body;

        bool _isFileBody;
        std::string _filePath;
        size_t _fileSize;

        std::string buildHeaderBlock(size_t contentLength) const;

    public:
        Response();
        void setStatus(int status);
        int getStatusCode() const;
        void setHeader(const std::string &key, const std::string &value);
        void setBody(const std::string &content);

        void setFileBody(const std::string &path, size_t fileSize);
        bool isFileBody() const;
        const std::string &getFilePath() const;
        size_t getFileSize() const;
        const std::string &getBody() const;

        std::string build() const;
        std::string buildHeaders() const;
};
    
std::string intToString(size_t value);

#endif