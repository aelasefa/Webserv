#include "CGI.hpp"

CGI::CGI()
{
}

CGI::CGI(const CGI &src)
{
	*this = src;
}

CGI::~CGI()
{
}

CGI &CGI::operator=(const CGI &rhs)
{
	if (this != &rhs)
	{
		_env = rhs._env;
	}
	return *this;
}

void CGI::_setEnv(const Request &request)
{
	(void)request;
}

std::string CGI::execute(const Request &request, const std::string &scriptPath)
{
	(void)request;
	(void)scriptPath;
	return "";
}
