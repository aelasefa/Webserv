#include "Response.hpp"
#include <sstream>

Response::Response() : _statusCode(200), _statusMessage("OK"), _body("")
{
}

Response::Response(const Response &src)
{
	*this = src;
}

Response::~Response()
{
}

Response &Response::operator=(const Response &rhs)
{
	if (this != &rhs)
	{
		_statusCode = rhs._statusCode;
		_statusMessage = rhs._statusMessage;
		_headers = rhs._headers;
		_body = rhs._body;
	}
	return *this;
}

void Response::setStatusCode(int code)
{
	_statusCode = code;
}

void Response::setBody(const std::string &body)
{
	_body = body;
}

void Response::setHeader(const std::string &key, const std::string &value)
{
	_headers[key] = value;
}

std::string Response::build() const
{
	std::ostringstream response;
	response << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
	
	std::map<std::string, std::string>::const_iterator it;
	for (it = _headers.begin(); it != _headers.end(); ++it)
	{
		response << it->first << ": " << it->second << "\r\n";
	}
	response << "\r\n" << _body;
	return response.str();
}
