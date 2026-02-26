#include "Request.hpp"

Request::Request() : _method(""), _path(""), _version(""), _body("")
{
}

Request::Request(const std::string &rawRequest)
{
	parse(rawRequest);
}

Request::Request(const Request &src)
{
	*this = src;
}

Request::~Request()
{
}

Request &Request::operator=(const Request &rhs)
{
	if (this != &rhs)
	{
		_method = rhs._method;
		_path = rhs._path;
		_version = rhs._version;
		_headers = rhs._headers;
		_body = rhs._body;
	}
	return *this;
}

void Request::parse(const std::string &rawRequest)
{
	(void)rawRequest;
}

std::string Request::getMethod() const
{
	return _method;
}

std::string Request::getPath() const
{
	return _path;
}

std::string Request::getBody() const
{
	return _body;
}
