#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>

class Response
{
public:
	Response();
	Response(const Response &src);
	~Response();
	Response &operator=(const Response &rhs);

	void setStatusCode(int code);
	void setBody(const std::string &body);
	void setHeader(const std::string &key, const std::string &value);
	std::string build() const;

private:
	int _statusCode;
	std::string _statusMessage;
	std::map<std::string, std::string> _headers;
	std::string _body;
};

#endif
