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

        // Marks this response as backed by a file on disk instead of an
        // in-memory body, so the caller can stream it in bounded chunks
        // instead of buffering the whole file in RAM.
        void setFileBody(const std::string &path, size_t fileSize);
        bool isFileBody() const;
        const std::string &getFilePath() const;
        size_t getFileSize() const;

        std::string build() const;
        // Headers only (no body) - used by the caller when the body will
        // be streamed separately from a file.
        std::string buildHeaders() const;
};
    
std::string intToString(size_t value);

#endif