#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

class Request
{
public:
	Request();
	Request(const std::string &rawRequest);
	Request(const Request &src);
	~Request();
	Request &operator=(const Request &rhs);

	void parse(const std::string &rawRequest);
	std::string getMethod() const;
	std::string getPath() const;
	std::string getBody() const;

private:
	std::string _method;
	std::string _path;
	std::string _version;
	std::map<std::string, std::string> _headers;
	std::string _body;
};

#endif
