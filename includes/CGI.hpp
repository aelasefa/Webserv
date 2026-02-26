#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <map>
#include "Request.hpp"

class CGI
{
public:
	CGI();
	CGI(const CGI &src);
	~CGI();
	CGI &operator=(const CGI &rhs);

	std::string execute(const Request &request, const std::string &scriptPath);

private:
	std::map<std::string, std::string> _env;
	void _setEnv(const Request &request);
};

#endif
