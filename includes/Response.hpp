#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>

class Response {
    private:
        int statusCode;
        std::string statusMessage;
        std::map<std::string, std::string> headers;
        std::string body;
    public:
        Response();
        void setStatus(int status);
        void setHeader(const std::string& key, const std::string& value);
        void setBody(const std::string& content);
        std::string build() const;
};

#endif